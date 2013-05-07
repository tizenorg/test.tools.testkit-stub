#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

#include <json/json.h>
#include "comfun.h"
#include "httpserver.h"

// if use 8080 for SERVER_PORT, it will not work with chrome
#define SERVER_PORT 8000
#define MAX_BUF 102400

#define GET   0
#define HEAD  2
#define POST  3
#define BAD_REQUEST -1

#if defined(DEBUG) | defined(_DEBUG)
    #ifndef DBG_ONLY
      #define DBG_ONLY(x) do { x } while (0)         
    #endif
#else
    #ifndef DBG_ONLY
      #define DBG_ONLY(x) 
    #endif
#endif

HttpServer::HttpServer() {
	m_test_cases = NULL;
	m_exeType = "auto"; // set default to auto
	m_totalBlocks = 0;
	m_current_block_index = 1;
	m_totalcaseCount = 0;
	m_block_case_index = 0;
	m_block_case_count = 0;
	m_total_case_index = 0;
	m_last_auto_result = "N/A";
	m_running_session = "";
	m_server_checked = false;
	g_show_log = false;
	g_port = "";
	g_hide_status = "";
	g_test_suite = "";
	g_exe_sequence = "";
	g_run_wiget = false;
	g_enable_memory_collection = "";
	m_block_finished = false;
	m_set_finished = false;
	m_timeout_count = 0;
	m_failto_launch = 0;
	m_killing_widget = false;
	m_rerun = false;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = timer_handler;
	sigaction(SIGALRM, &sa, NULL);
}

HttpServer::~HttpServer() {
	if (m_test_cases) {
		delete[] m_test_cases;
		m_test_cases = NULL;
	}
	DBG_ONLY(outputFile.close(););
}

// generate response code. send response
void HttpServer::sendresponse(int s, int code, struct HttpRequest *prequest,
		string content) {
	string buffer;
	stringstream len_stream;
	len_stream << content.length();
	prequest->responsecode = code;
	// generate response head
	switch (code) {
	case 200: {
		buffer =
				"HTTP/1.1 200 OK\r\nServer: testkit-stub/1.0\r\nContent-Type: "
						+ prequest->prefix
						+ "\r\nAccept-Ranges: bytes\r\nContent-Length: "
						+ len_stream.str()
						+ "\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n"
						+ content;
		break;
	}
	default:
		break;
	}

	if (g_show_log) cout << buffer << endl;
	// send out the http response
	send(s, buffer.c_str(), buffer.length(), 0);
}

// parse the http request
int HttpServer::getrequest(string requestbuf, struct HttpRequest *prequest) {
	std::vector < std::string > splitstr = ComFun::split(requestbuf, " ");
	if (splitstr.size() >= 2) {
		prequest->method = splitstr[0];
		prequest->path = splitstr[1];
	}

	if (prequest->path.find('?') == string::npos) {
		//get the com module send data
		int content_index = requestbuf.find("\r\n\r\n", 0);
		if (content_index > -1) {
			prequest->content = requestbuf.substr(
					content_index + strlen("\r\n\r\n"));
		}
	} else {
		int session_index = prequest->path.find("?");
		prequest->content = prequest->path.substr(session_index + 1);
	}
	if (prequest->method == "GET") {
		return GET;
	} else if (prequest->method == "POST") {
		return POST;
	}
	return -1;
}

// parse the test case data sent by com-module with init_test cmd
void HttpServer::parse_json_str(string case_node) {
	Json::Reader reader;
	Json::Value value;

	bool parsed = reader.parse(case_node, value);
	if (!parsed) // try to parse as a file if parse fail
	{ // "test.json" is for verify
		cout << case_node << endl;
		std::ifstream test(case_node.c_str(), std::ifstream::binary);
		parsed = reader.parse(test, value, false);
	}

	if (parsed) {
		m_totalBlocks = atoi(value["totalBlk"].asString().c_str());
		m_current_block_index = atoi(value["currentBlk"].asString().c_str());
		m_totalcaseCount = atoi(value["casecount"].asString().c_str());
		m_exeType = value["exetype"].asString();

		const Json::Value arrayObj = value["cases"];
		m_block_case_count = arrayObj.size();

		if (m_test_cases) {
			delete[] m_test_cases;
			m_test_cases = NULL;
		}
		m_test_cases = new TestCase[m_block_case_count];

		for (int i = 0; i < m_block_case_count; i++) {
			m_test_cases[i].init(arrayObj[i]);
		}
	}
}

void HttpServer::cancel_time_check() {
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL); // set timer with value 0 will stop the timer
	// refer to http://linux.die.net/man/2/setitimer, each process only have 1 timer of the same type.
}

// set timeout value to 90 seconds for each case
void HttpServer::set_timer(int timeout_value) {
	timer.it_value.tv_sec = timeout_value;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	int ret = setitimer(ITIMER_REAL, &timer, NULL);
	if (ret < 0)
		perror("error: set timer!!!");
}


/** send out response according to different http request. 
a typical workflow of auto case would be
/check_server_status						(by com-module)
/init_test									(by com-module)
/check_server 								(by widget)
/init_session_id?session_id=2033			(by widget)
/auto_test_task?session_id=2033				(by widget)
/check_execution_progress?session_id=2033	(by widget)
/ask_next_step?session_id=2033				(by widget)
/commit_result 								(by widget)
/auto_test_task?session_id=2033				(by widget)
/manual_cases 								(by widget)
/check_server_status 						(by com-module)
/get_test_result 							(by com-module)
/shut_down_server 							(by com-module)


a typical workflow of manual case would be
/check_server_status						(by com-module)
/init_test									(by com-module)
/check_server 								(by widget)
/init_session_id?session_id=2033			(by widget)
/auto_test_task?session_id=2033				(by widget)
/manual_cases 								(by widget)
/check_server_status 						(by com-module)
/commit_manual_result 						(by widget)
...
/check_server_status 						(by com-module)
/commit_manual_result 						(by widget)
/generate_xml 								(by widget)
/get_test_result 							(by com-module)
/shut_down_server 							(by com-module)

**/
void HttpServer::processpost(int s, struct HttpRequest *prequest) {
	prequest->prefix = "application/json";
	string json_str = "";
	string json_parse_str = "";

	if (prequest->path.find("/init_test") != string::npos) {// invoke by com-module to send test data
		m_block_finished = false;
		m_set_finished = false;
		m_timeout_count = 0;
		m_server_checked = false;
		cout << "[ init the test suite ]" << endl;
		DBG_ONLY(outputFile << "[ init the test suite ]" << endl;);
		parse_json_str(prequest->content);

		start_client();
		//set_timer(30); // set timer here incase widget hang.

		m_block_case_index = 0;
		if (m_current_block_index == 1)
			m_total_case_index = 0;

		json_str = "{\"OK\":1}";
	} else if (prequest->path == "/check_server") {// invoke by index.html to find server running or not
		cout << "[ checking server, and found the server is running ]" << endl;
		DBG_ONLY(outputFile << "[ checking server, and found the server is running ]" << endl;);
		m_server_checked = true;
		json_str = "{\"OK\":1}";
	} else if (prequest->path == "/check_server_status") {// invoke by com-module to get server status
		Json::Value status;
		status["block_finished"] = m_block_finished ? 1 : 0;
		status["finished"] = m_set_finished ? 1 : 0;
        if (m_failto_launch > 10) {
            status["error_code"] = 2;
            status["finished"] = 1; // finish current set if can't launch widget
        }
		json_str = status.toStyledString();

       	if (!m_server_checked) {
       		cout << "waiting for widget check_server" << endl;
       		DBG_ONLY(outputFile << "wait for widget check_server" << endl;);
       	}
		if (m_totalBlocks > 0)
		{
			cout << "block: " << m_current_block_index << "/" << m_totalBlocks << ", total case: " << m_total_case_index << "/" << m_totalcaseCount << ", block case: " << m_block_case_index << "/" << m_block_case_count << ", m_timeout_count:" << m_timeout_count << endl;
        	DBG_ONLY(outputFile << "block: " << m_current_block_index << "/" << m_totalBlocks << ", total case: " << m_total_case_index << "/" << m_totalcaseCount << ", block case: " << m_block_case_index << "/" << m_block_case_count << ", m_timeout_count:" << m_timeout_count << endl;);
        	if (m_exeType != "auto") {
        		cout << "manual cases. please check device." << endl << endl;
        		DBG_ONLY(outputFile << "manual cases. please click on device." << endl << endl;);
        	}
		}
        
	} else if (prequest->path == "/shut_down_server") {
		if (g_run_wiget == true)
			killAllWidget(); // kill all widget when shutdown server
		json_str = "{\"OK\":1}";
		gIsRun = 0;
	} else if (prequest->path.find("/init_session_id") != string::npos) {// invoke by index.html to record a session id
		json_str = "{\"OK\":1}";
		int index = prequest->path.find('=');
		if (index != -1) {
			m_running_session = prequest->path.substr(index + 1);
			cout << "[ sessionID: " << m_running_session << " is gotten from the client ]" << endl;
			DBG_ONLY(outputFile << "[ sessionID: " << m_running_session << " is gotten from the client ]" << endl;);
		} else {
			cout << "[ invalid session id ]" << endl;
			DBG_ONLY(outputFile << "[ invalid session id ]" << endl;);
		}
	} else if (prequest->path.find("/ask_next_step") != string::npos) {// invoke by index.html to check whether there are more cases
		if (m_block_finished || m_set_finished)
			json_str = "{\"step\":\"stop\"}";
		else
			json_str = "{\"step\":\"continue\"}";

		m_timeout_count = 0; // reset the timeout count
	} else if (prequest->path.find("/auto_test_task") != string::npos) {// invoke by index.html to get current auto case
		if (m_test_cases == NULL) {
			json_str = "{\"Error\":\"no case\"}";
		} else if (m_exeType != "auto") {
			json_str = "{\"none\":0}";
		} else {
			string error_type = "";
			bool find_tc = get_auto_case(prequest->content, &error_type);
			if (find_tc == false) {
				json_str = "{\"" + error_type + "\":0}";
			} else {
				json_str =
						m_test_cases[m_block_case_index].to_json().toStyledString();
			}
		}
	} else if (prequest->path.find("/manual_cases") != string::npos) {// invoke by index.html to get all manual cases
		cancel_time_check(); // should not timeout in manual mode
		if (!m_test_cases) {
			json_str = "{\"Error\":\"no case\"}";
		} else if (m_exeType == "auto") {
			json_str = "{\"none\":0}";
		} else {
			Json::Value arrayObj;
			for (int i = 0; i < m_block_case_count; i++)
				arrayObj.append(m_test_cases[i].to_json());

			json_str = arrayObj.toStyledString();
		}
	} else if (prequest->path.find("/case_time_out") != string::npos) {// invoke by timer to notify case timeout
		if (!m_test_cases) {
			json_str = "{\"Error\":\"no case\"}";
		} else if (m_block_case_index < m_block_case_count) {
			checkResult(&m_test_cases[m_block_case_index]);
			json_str = "{\"OK\":\"timeout\"}";
		} else {
			json_str = "{\"Error\":\"case out of index\"}";
		}
	} else if (prequest->path.find("/commit_manual_result") != string::npos) {// invoke by index.html to provide result of a manual case.
		if ((prequest->content.length() == 0) || (!m_test_cases)) {
			json_str = "{\"Error\":\"no manual result\"}";
		} else {
			find_purpose(prequest, false); // will set index in find_purpose
			json_str = "{\"OK\":1}";
		}
	} else if (prequest->path.find("/check_execution_progress") != string::npos) {//invoke by index.html to get test result of last auto case
		char *total_count = new char[16];
		sprintf(total_count, "%d", m_totalcaseCount);
		char *current_index = new char[16];
		sprintf(current_index, "%d", m_total_case_index + 1);

		string count_str(total_count);
		string index_str(current_index);
		json_str = "{\"total\":" + count_str + ",\"current\":" + index_str
				+ ",\"last_test_result\":\"" + m_last_auto_result + "\"}";

		delete[] total_count;
		delete[] current_index;
	}
	//generate_xml:from index_html, a maually block finished when click done in widget
	else if (prequest->path == "/generate_xml") {
		cancel_time_check();
		m_block_finished = true;
		if (m_current_block_index == m_totalBlocks)
			m_set_finished = true;

		json_str = "{\"OK\":1}";
	}
	//from com module,when m_set_finished is true
	else if (prequest->path == "/get_test_result") {
		cancel_time_check();
		if (!m_test_cases) {
			json_str = "{\"Error\":\"no case\"}";
		} else {
			Json::Value root;
			Json::Value arrayObj;
			for (int i = 0; i < m_block_case_count; i++)
				arrayObj.append(m_test_cases[i].result_to_json());

			char count[8];
			memset(count, 0, 8);
			sprintf(count, "%d", m_block_case_count);
			root["count"] = count;
			root["cases"] = arrayObj;

			json_str = root.toStyledString();
		}
	}
    // index.html invoke this with purpose and result of an auto case, auto case commit result. 
    // we need find correct test case by purpose, and record test result to it.
    else if (prequest->path == "/commit_result") {
		if ((prequest->content.length() == 0) || (!m_test_cases)) {
			json_str = "{\"Error\":\"no result\"}";
			m_last_auto_result = "BLOCK";
			m_rerun = true;
		} else {
			char* tmp = ComFun::UrlDecode(prequest->content.c_str());
			string content = tmp;
			delete[] tmp; // free memory from comfun
			string sessionid = "";
			std::vector < std::string > splitstr = ComFun::split(content, "&");
			for (unsigned int i = 0; i < splitstr.size(); i++) {
				vector < string > resultkey = ComFun::split(splitstr[i], "=");
				if (resultkey[0] == "session_id"){
					sessionid = resultkey[1];
					break;
				}
			}
			if (m_running_session == sessionid) {
				m_rerun = false;
				if (m_block_case_index < m_block_case_count) {
	            	m_block_case_index++;
	            	m_total_case_index++;
	            }

	            find_purpose(prequest, true);

	            json_str = "{\"OK\":1}";
            }
        }
    } else if (prequest->path == "/set_capability") {// by com-module to send capability data
        Json::Reader reader;

        reader.parse(prequest->content, m_capability);

        json_str = "{\"OK\":1}";
    } else if (prequest->path.find("/capability") != string::npos) {// by test suite. only one query parameter each time
        char* tmp = ComFun::UrlDecode(prequest->content.c_str());
        string content = tmp;
        delete[] tmp; // free memory from comfun

        json_str = "{\"support\":0}";
        string name = "", value = "";
        std::vector < std::string > splitstr = ComFun::split(content, "&");
        for (unsigned int i = 0; i < splitstr.size(); i++) {
            vector < string > resultkey = ComFun::split(splitstr[i], "=");
            if (resultkey[0] == "name") {
            	name = resultkey[1];
            	for (unsigned int i = 0; i < name.size(); i++)
            		name[i] = tolower(name[i]);
            }
            if (resultkey[0] == "value") value = resultkey[1];
        }
        if (m_capability[name].isBool()) {// for bool value, omit the value part
            json_str = "{\"support\":1}";
        }
        else if (m_capability[name].isInt()) {
            if (m_capability[name].asInt() == atoi(value.c_str()))
                json_str = "{\"support\":1}";
        }
        else if (m_capability[name].isString()) {
            if (m_capability[name].asString() == value)
                json_str = "{\"support\":1}";
        }
	} else {
        cout << "=================unknown request: " << prequest->path << endl;
	}
    
    DBG_ONLY(
		outputFile << "prequest->path is:" << prequest->path << endl;
		outputFile << "prequest->content is:" << prequest->content << endl;
		outputFile << "server response is:" << json_str << endl;
    );
    if (g_show_log == true)
    {
		cout << "prequest->path is:" << prequest->path << endl;
		cout << "prequest->content is:" << prequest->content << endl;
		cout << "server response is:" << json_str << endl;
    }

	if (json_str != "")
		sendresponse(s, 200, prequest, json_str);
}

// find correct case according the purpose sent by widget
void HttpServer::find_purpose(struct HttpRequest *prequest, bool auto_test) {
	string purpose = "";
	string result = "";
	string msg = "";
	string content = "";

	char* tmp = ComFun::UrlDecode(prequest->content.c_str());
	content = tmp;
	delete[] tmp; // free memory from comfun

	std::vector < std::string > splitstr = ComFun::split(content, "&");
	for (unsigned int i = 0; i < splitstr.size(); i++) {
		vector < string > resultkey = ComFun::split(splitstr[i], "=");
		if (resultkey[0] == "purpose")
			purpose = resultkey[1];
		else if (resultkey[0] == "result")
			result = resultkey[1];
		else if (resultkey[0] == "msg")
			msg = resultkey[1];
	}

	bool found = false;
	for (int i = 0; i < m_block_case_count; i++) {
		if (m_test_cases[i].purpose == purpose) {
			m_test_cases[i].set_result(result, msg);
			found = true;
			if (!auto_test) // should auto test use this logic?
				m_block_case_index = i; // set index by purpose found
			break;
		}
	}
	if (!found) {
		cout << "[ Error: can't find any test case by key: " << purpose << " ]"	<< endl;
		DBG_ONLY(outputFile << "[ Error: can't find any test case by key: " << purpose << " ]"	<< endl;);
	}

	if (auto_test)
		m_last_auto_result = result;
}

// create new thread for each http request
void* processthread(void *para) {
	string recvstr = "";
	char *buffer = new char[MAX_BUF]; // suppose 1 case need 1k, 100 cases will be sent each time, we need 100k memory?
	memset(buffer, 0, MAX_BUF);
	long iDataNum = 0;
	int recvnum = 0;
	HttpServer *server = (HttpServer *) para;
	struct HttpRequest httprequest;
	httprequest.content = "";
	httprequest.path = "";

	int content_length = 0;
	int real_content_length = 0;
	while (1) {
		iDataNum = recv(server->clientsocket, buffer + recvnum,
				MAX_BUF - recvnum - 1, 0);
		if (iDataNum <= 0) {
			delete[] buffer;
			close(server->clientsocket);
			pthread_exit (NULL);
			return 0;
		}
		recvnum += iDataNum;

		// to calculate content length and real content read
		if (content_length == 0) {
			char* Content_Length = strstr(buffer, "Content-Length: ");
			if (Content_Length != NULL) {
				char tmplen[32];
				sscanf(Content_Length, "%[^:]:%d", tmplen, &content_length);
				if (content_length > 0) {
					char* tmp = strstr(buffer, "\r\n\r\n");
					if (tmp == NULL) {
						tmp = strstr(buffer, "\n\n");
						if (tmp != NULL)
							real_content_length = iDataNum - (tmp-buffer) -2;
						else {
							cout << "[ can't find \\r\\n\\r\\n or \\n\\n ]" << endl;
							break;
						}
					}
					else real_content_length = iDataNum - (tmp-buffer) - 4;
				}
			}
			else real_content_length += iDataNum;
		}
		else real_content_length += iDataNum;

		if (real_content_length >= content_length) break;
	}
	buffer[recvnum] = '\0';
	recvstr = buffer;
	delete[] buffer;

	// parse request and process it
	switch (server->getrequest(recvstr, &httprequest)) {
	case GET:
	case POST:
		server->processpost(server->clientsocket, &httprequest);
		break;
	default:
		break;
	}
	close(server->clientsocket);
	pthread_exit (NULL);
	return 0;
}

// send out timeout request to server if timeout. timer handler must be a global function, so can't invoke server's member function directly. 
void timer_handler(int signum) {
	// when timeout, we send a http request to server, so server can check result. seems no other convinent way to do it.
	const char *strings =
			"GET /case_time_out HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: Close\r\n\r\n";
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(SERVER_PORT);
	int len = sizeof(address);
	int result = connect(sockfd, (struct sockaddr *) &address, len);
	if (result == -1) {
		cout << "fail to send timeout request to server!" << endl;
	} else {
		write(sockfd, strings, strlen(strings));
		close(sockfd);
	}
}

void HttpServer::print_info_string(string case_id) {
	if (m_rerun) {
		cout << "\n[case] execute case again: ";
		DBG_ONLY(outputFile << "\n[case] execute case again: ";);
	}
	else {
		cout << "\n[case] execute case: ";
		DBG_ONLY(outputFile << "\n[case] execute case: ";);
	}
	cout << case_id << endl;
	cout << "last_test_result: " << m_last_auto_result << endl;
	DBG_ONLY(outputFile << case_id << endl;);
	DBG_ONLY(outputFile << "last_test_result: " << m_last_auto_result << endl;);
}

// prepare to run current auto case by set the start time, etc.
bool HttpServer::get_auto_case(string content, string *type) {
	if (!m_killing_widget) {
		if (content != "") {
			string value = content.substr(content.find("=") + 1);
			if (value.length() > 0) {
				if (m_running_session == value) {
					if (m_block_case_index < m_block_case_count) {
						set_timer(m_test_cases[m_block_case_index].timeout_value);
						m_test_cases[m_block_case_index].set_start_at();
						print_info_string(m_test_cases[m_block_case_index].case_id);
						return true;
					} else {
						cout << endl << "[ no auto case is available any more ]" << endl;
						DBG_ONLY(outputFile << endl << "[ no auto case is available any more ]" << endl;);
						*type = "none";
						m_block_finished = true;
						if (m_current_block_index == m_totalBlocks)
							m_set_finished = true; // the set is finished if current block is the last block
					}
				} else {
					cout << "[ Error: invalid session ID ]" << endl;
					DBG_ONLY(outputFile << "[ Error: invalid session ID ]" << endl;);
					*type = "invalid";
				}
			}
		}
	} else {
		cout << "\n[ restart client process is activated, exit current client ]" << endl;
		DBG_ONLY(outputFile << "\n[ restart client process is activated, exit current client ]" << endl;);
		*type = "stop";
	}
	return false;
}

//start the socket server, listen to client
void HttpServer::StartUp() {
    DBG_ONLY(
	outputFile.open("httpserver_log.txt",ios::out);
	outputFile<<"httpserver.g_port is:"+g_port<<endl;
	outputFile<<"httpserver.g_hide_status is:"+g_hide_status<<endl;
	outputFile<<"httpserver.g_test_suite is:"+g_test_suite<<endl;
	outputFile<<"httpserver.g_exe_sequence is:"+g_exe_sequence<<endl;
	outputFile<<"httpserver.g_enable_memory_collection is:"+g_enable_memory_collection<<endl;
    );
    if (g_show_log == true)
    {
		cout<<"httpserver.g_show_log is:"<<g_show_log<<endl;
		cout<<"httpserver.g_port is:"+g_port<<endl;
		cout<<"httpserver.g_hide_status is:"+g_hide_status<<endl;
		cout<<"httpserver.g_test_suite is:"+g_test_suite<<endl;
		cout<<"httpserver.g_exe_sequence is:"+g_exe_sequence<<endl;
		cout<<"httpserver.g_enable_memory_collection is:"+g_enable_memory_collection<<endl;
    }
    if (g_run_wiget == true){ 
		getAllWidget();
		killAllWidget();
    }
    else //wait for the index window.close, otherwise will occur bind aleady error
		sleep(5);
    int serversocket;
	gServerStatus = 1;
	struct sockaddr_in server_addr;
	struct sockaddr_in clientAddr;
	int addr_len = sizeof(clientAddr);

	if ((serversocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror( "error: create server socket!!!");
		return;
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int tmp = 1;
	setsockopt(serversocket, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int));
	if (bind(serversocket, (struct sockaddr *) &server_addr,
			sizeof(server_addr)) < 0) {
		perror("error: bind address !!!!");
		return;
	}

	if (listen(serversocket, 5) < 0) {
		perror("error: listen !!!!");
		return;
	}
	gIsRun = 1;
	cout << "[Server is running.....]" << endl;
	DBG_ONLY(outputFile << "[Server is running.....]" << endl;);

	while (gIsRun) {
		clientsocket = accept(serversocket, (struct sockaddr *) &clientAddr,
				(socklen_t*) &addr_len);
		if (clientsocket < 0) {
			perror("error: accept client socket !!!");
			continue;
		}
		if (gServerStatus == 0) {
			close(clientsocket);
		} else if (gServerStatus == 1) {
			pthread_t threadid;
			pthread_create(&threadid, NULL, processthread, (void *) this);
			//must have,otherwise the thread can not exit
			if (threadid != 0) { // can't exit thread without below code?
				pthread_join(threadid, NULL);
			}
		}
	}
	close(serversocket);
}

// set result to current if timeout
void HttpServer::checkResult(TestCase* testcase) {
	if (!testcase->is_executed) {
		cout << "[ Warning: time is out, test case \"" << testcase->purpose	<< "\" is timeout, set the result to \"BLOCK\", and restart the client ]" << endl;
		DBG_ONLY(outputFile << "[ Warning: time is out, test case \"" << testcase->purpose	<< "\" is timeout, set the result to \"BLOCK\", and restart the client ]" << endl;);

		testcase->set_result("BLOCK", "Time is out");
		m_last_auto_result = "BLOCK";

		if (g_run_wiget == true){
			cout << "[ start new client in 5sec ]" << endl;
			DBG_ONLY(outputFile << "[ start new client in 5sec ]" << endl;);
			sleep(5);
			start_client(); // start widget again in case it dead. browser not need to restart
		}
		m_timeout_count++;
		if (m_block_case_index < m_block_case_count) {
			m_block_case_index++;
			m_total_case_index++;
		}
	} else {
		cout << "[ test case \"" << testcase->purpose << "\" is executed in time, and the result is testcase->result ]" << endl;
		DBG_ONLY(outputFile << "[ test case \"" << testcase->purpose << "\" is executed in time, and the result is testcase->result ]" << endl;);
	}

}

void HttpServer::getAllWidget() {
	char buf[128];
	memset(buf, 0, 128);
	FILE *pp;
	string cmd = g_launcher+" -l | awk '{print $NF}' | sed -n '3,$p'";
	if ((pp = popen(cmd.c_str(), "r")) == NULL) {
		cout << "popen() error!" << endl;
		DBG_ONLY(outputFile << "popen() error!" << endl;);
		return;
	}

	while (fgets(buf, sizeof buf, pp)) {
		buf[strlen(buf) - 1] = 0; // remove the character return at the end.
		m_widgets.push_back(buf);
		memset(buf, 0, 128);
	}
	pclose(pp);
}

// try to kill all widget listed by wrt-launch -l
void HttpServer::killAllWidget() {
	m_killing_widget = true;
	string cmd = "killall " + g_launcher + " 2>/dev/null";
	system(cmd.c_str());

	char buf[128];
	memset(buf, 0, 128);
	FILE *pp;
	if ((pp = popen("ps ax | awk '{print $NF}' | sed -n '2,$p'", "r")) == NULL) {
		cout << "popen() error!" << endl;
		DBG_ONLY(outputFile << "popen() error!" << endl;);
		return;
	}

	std::vector<string> processes;
	while (fgets(buf, sizeof buf, pp)) {
		if(g_show_log == true)
			cout<<buf<<endl;
		buf[strlen(buf) - 1] = 0; // remove the character return at the end.
		processes.push_back(buf);
		memset(buf, 0, 128);
	}
	if(g_show_log == true)
		cout<<"end while"<<endl;
	pclose(pp);


	for (unsigned int i = 0; i < m_widgets.size(); i++) 
		for (unsigned int j = 0; j < processes.size(); j++)
			if (string::npos != processes[j].find(m_widgets[i])) {
				string cmd = g_launcher +" -k " + m_widgets[i]; // use wrt-launcher to kill widget
				DBG_ONLY(outputFile<<cmd<<endl;);
				if(g_show_log == true)
					cout<<cmd<<endl;
				run_cmd(cmd, "result: killed", true);
				break;
			}
	if(g_show_log == true)
		cout<<"end for"<<endl;
	m_killing_widget = false;
}

// try to start widget
void HttpServer::start_client() {
	if (m_set_finished) return;// not restart widget if set finished
	if(g_run_wiget == true){
	    run_cmd(g_kill_cmd, "result: killed", true);// kill before launch incase sometime relaunch fail, widget hang
		sleep(5);
		while (!run_cmd(g_launch_cmd, "result: launched", true)) {
			m_failto_launch++;
			DBG_ONLY(outputFile << m_failto_launch << endl;);
			if(g_show_log == true)
				cout << m_failto_launch << endl;
			if (m_failto_launch > 10) {
	            m_set_finished = true;
	            m_block_finished = true;
	            m_total_case_index = m_totalcaseCount;
				break;
			}
	        run_cmd(g_kill_cmd, "result: killed", true);
			sleep(10); // try until start widget success
		}
		m_failto_launch = 0;
	}
	else{
		sleep(5); // sleep 5 seconds to avoid launch browser too frequently, otherwise may launch fail.
		string launch_cmd = ""; 
		launch_cmd = g_launcher +"&";
		int status = system(launch_cmd.c_str());
		if(status != 0)
		{
		    cout<<"[cmd: "<<launch_cmd<<" error]"<<endl; 
		    return;
		}
	}
}

// run shell cmd. return true if the output equal to expectString. show cmd and output if showcmdAnyway.
bool HttpServer::run_cmd(string cmdString, string expectString,
		bool showcmdAnyway) {
	bool ret = false;

	char buf[128];
	memset(buf, 0, 128);
	FILE *pp;
	if ((pp = popen(cmdString.c_str(), "r")) == NULL) {
		cout << "popen() error!" << endl;
		DBG_ONLY(outputFile << "popen() error!" << endl;);
		return ret;
	}

	while (fgets(buf, sizeof buf, pp)) {
		if (strstr(buf, expectString.c_str()))
			ret = true;
		if (ret || showcmdAnyway) { // show cmd and result if ret or showcmdAnyWay
			cout << cmdString << endl;
			cout << buf << endl;
			DBG_ONLY(outputFile << cmdString << endl;);
			DBG_ONLY(outputFile << buf << endl;);
		}
		memset(buf, 0, 128);		
	}
	pclose(pp);	
	return ret;
}