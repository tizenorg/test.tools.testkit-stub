/*
 * Copyright (c) 2013-2014 Intel Corporation, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sstream>

#include "httpserver.h"

#if defined(__WIN32__) || defined(__WIN64__)
# include <winsock2.h>
# define sleep(n) Sleep(1000 * (n))
	struct Timer_thread_info {	/* Used as argument to thread */
		pthread_t thread_id;	/* ID returned by pthread_create() */
		int timeout;
	};
	Timer_thread_info timer_thread_info;

void* win_timer_callback(void* para) {
	Timer_thread_info *tinfo = (Timer_thread_info *) para;
	pthread_testcancel();
	sleep(tinfo->timeout);
	pthread_testcancel();
	timer_handler(0);
	pthread_exit (NULL);
	return 0;
}

#else
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <unistd.h>
# include <arpa/inet.h>
  struct sigaction sa;
  struct itimerval timer;
#endif
  
#include <json/json.h>
#include "comfun.h"

#define MAX_BUF 300000

#define GET   0
#define HEAD  2
#define POST  3
#define BAD_REQUEST -1

extern bool g_show_log;
bool g_openlog_fail = false;
#ifndef DBG_ONLY
	#define DBG_ONLY(x) {if (g_openlog_fail) cout << x << endl; else outputFile << x << endl;}
#endif

#if defined(DEBUG) | defined(_DEBUG)    
		#define SHOW_FORCE true
#else
		#define SHOW_FORCE false
#endif

ofstream outputFile;
HttpServer httpserver;
int serversocket;

struct thread_info {    /* Used as argument to thread */
    pthread_t thread_id;/* ID returned by pthread_create() */
	int clientsocket;
    HttpServer *server;
};

pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

int g_error_code = 0;

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
	m_check_times = 0;
	g_port = 8000; // if use 8080 for server_port, it will not work with chrome
	g_run_wiget = false;
	g_launcher = "";
	m_block_finished = true;
	m_set_finished = true;
	m_timeout_count = 0;
	m_failto_launch = 0;
	m_max_fail_launch = 3;
	m_killing_widget = false;
	m_suite_name = "";
	m_suite_id = "";

#if defined(__WIN32__) || defined(__WIN64__)
#else
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = timer_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, NULL);
#endif

	if(SHOW_FORCE) g_show_log = true;	
}

HttpServer::~HttpServer() {
	if (m_test_cases) {
		delete[] m_test_cases;
		m_test_cases = NULL;
	}
	if(g_show_log) outputFile.close();
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
		if(g_show_log) DBG_ONLY(case_node);
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
#if defined(__WIN32__) || defined(__WIN64__)
	if (timer_thread_info.thread_id.p) {
		pthread_cancel(timer_thread_info.thread_id);
		memset(&timer_thread_info.thread_id, 0, sizeof(timer_thread_info.thread_id));
	}
#else
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	int ret = setitimer(ITIMER_REAL, &timer, NULL); // set timer with value 0 will stop the timer
	if ((ret < 0) && (g_show_log))
		DBG_ONLY("fail to cancel timer !!!");
	// refer to http://linux.die.net/man/2/setitimer, each process only have 1 timer of the same type.
#endif
}

// set timeout value to 90 seconds for each case
void HttpServer::set_timer(int timeout_value) {
#if defined(__WIN32__) || defined(__WIN64__)
	cancel_time_check(); // clear timer if any
	timer_thread_info.timeout = timeout_value;
	pthread_create(&timer_thread_info.thread_id, NULL, win_timer_callback, &timer_thread_info);
#else
	timer.it_value.tv_sec = timeout_value;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	int ret = setitimer(ITIMER_REAL, &timer, NULL);
	if ((ret < 0) && (g_show_log))
		DBG_ONLY("fail to set timer !!!");
#endif
}

Json::Value HttpServer::splitContent(string content) {
	Json::Value httpParas;
	if (content != "") {
		std::vector < std::string > splitstr = ComFun::split(content, "&");
		for (unsigned int i = 0; i < splitstr.size(); i++) {
			vector < string > resultkey = ComFun::split(splitstr[i], "=");
			char* tmp = ComFun::UrlDecode(resultkey[0].c_str());
			string key = tmp;
			delete[] tmp; // free memory from comfun
			if (resultkey[1] != "") {
				tmp = ComFun::UrlDecode(resultkey[1].c_str());
				string value = tmp;
				delete[] tmp; // free memory from comfun
				httpParas[key] = value;
			}
			else httpParas["myContent"] = key;
		}
		if(g_show_log)
			DBG_ONLY(httpParas.toStyledString());
	}
	return httpParas;
}

/** send out response according to different http request. 
a typical workflow of auto case would be
/check_server_status						(by com-module)
/init_test									(by com-module)
/set_testcase								(by com-module)
/check_server 								(by widget)
/init_session_id?session_id=2033			(by widget)
/auto_test_task?session_id=2033				(by widget)
/check_execution_progress?session_id=2033	(by widget)
/ask_next_step?session_id=2033				(by widget)
/commit_result 								(by widget)
/auto_test_task?session_id=2033				(by widget)
/manual_cases 								(by widget)
/generate_xml 								(by widget)
/check_server_status 						(by com-module)
/get_test_result 							(by com-module)


a typical workflow of manual case would be
/check_server_status						(by com-module)
/init_test									(by com-module)
/set_testcase								(by com-module)
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

**/
void HttpServer::processpost(int s, struct HttpRequest *prequest) {
	prequest->prefix = "application/json";
	string json_str = "{\"OK\":1}";

#if defined(__WIN32__) || defined(__WIN64__)
	cout << "prequest->path is:" << prequest->path << endl;
	cout << "prequest->content is:" << prequest->content << endl;
#else
    if(g_show_log) {
    	DBG_ONLY("prequest->path is:" << prequest->path);
		DBG_ONLY("prequest->content is:" << prequest->content);
	}
#endif

	if (prequest->path.find("/init_test") != string::npos) {// invoke by com-module to init some para for test
		Json::Reader reader;
		Json::Value value;
		if(g_show_log)
			DBG_ONLY("[ init the test suite ]");

		bool parsed = reader.parse(prequest->content, value);
		if (parsed) {
			g_launcher = value["launcher"].asString();
			m_suite_name = value["suite_name"].asString();
			if (g_launcher == "wrt-launcher")
			{
				g_run_wiget = true;
				m_suite_id = value["suite_id"].asString();

				g_launch_cmd = g_launcher + " -s " + m_suite_id;
				g_kill_cmd = g_launcher + " -k " + m_suite_id;
				killAllWidget();
			}
			else if (g_launcher == "xwalk") {// xwalk on android
				g_run_wiget = false;
				//am start -n org.xwalk.tct-2dtransforms-css3-tests/.tct-2dtransforms-css3-testsActivity
				//g_launch_cmd = "am start -n org.xwalk." + m_suite_name + "/." + m_suite_name + "Activity";
				//g_kill_cmd = "am force-stop org.xwalk." + m_suite_name;
				g_launch_cmd = "";
				g_kill_cmd = "";
			}
			else
			{
#if defined(__WIN32__) || defined(__WIN64__)
				g_launch_cmd = "start /b " + g_launcher;
				g_kill_cmd = "";
#else
				g_launch_cmd = g_launcher + " &";
				if (g_launcher.find("xwalk") != string::npos) //kill xwalk
					g_kill_cmd = "pkill xwalk";
					//g_kill_cmd = "ps ax | grep xwalk | grep -v testkit-lite | grep -v grep | awk '{print $1F}' | xargs kill -9;";
				else g_kill_cmd = ""; // not kill browser
#endif
				g_run_wiget = false;
				//wait for the index window.close, otherwise will occur bind aleady error
				sleep(1);
			}
		}
		else {
			if(g_show_log){
				DBG_ONLY("error while parse para from com-module, can't start test");
				DBG_ONLY(prequest->content);
			}			
			json_str = "{\"Error\":\"parse error\"}";
		}
	} else if (prequest->path.find("/set_testcase") != string::npos) {// invoke by com-module to send testcase data
		m_block_finished = false;
		m_set_finished = false;
		m_timeout_count = 0;
		m_server_checked = false;
		if(g_show_log)
			DBG_ONLY("[ set test cases ]");
		parse_json_str(prequest->content);

		//start_client();
//		if(g_run_wiget == true)//not set timeout when run on browser
		set_timer(20); // set timer here incase widget hang.
		m_check_times = 0;

		m_block_case_index = 0;
		if (m_current_block_index == 1)
			m_total_case_index = 0;
	} else if (prequest->path == "/check_server") {// invoke by index.html to find server running or not
		m_server_checked = true;
		m_check_times = 0;
		cancel_time_check();
		if(g_show_log)
			DBG_ONLY("[ checking server, and found the server is running ]");
	} else if (prequest->path == "/check_server_status") {// invoke by com-module to get server status
		Json::Value status;
		status["block_finished"] = m_block_finished ? 1 : 0;
		status["finished"] = m_set_finished ? 1 : 0;
        if (m_failto_launch > m_max_fail_launch) {
            status["error_code"] = 2;
            status["finished"] = 1; // finish current set if can't launch widget
        }
        else if (g_error_code > 0) {
        	status["error_code"] = g_error_code;
        	g_error_code = 0;
        }

        pthread_mutex_lock( &result_mutex );
       	char count[8];
		memset(count, 0, 8);
		sprintf(count, "%d", m_result.size());
		status["count"] = count;
		status["cases"] = m_result;
		m_result.clear();
		pthread_mutex_unlock( &result_mutex );

		json_str = status.toStyledString();

       	if (!m_server_checked) {
       		if(g_show_log)
       			DBG_ONLY("wait for widget check_server, please check on device." << endl);
       	}
		if (m_totalBlocks > 0)
		{
			if(g_show_log)
        		DBG_ONLY("group: " << m_current_block_index << "/" << m_totalBlocks << ", total case: " << m_total_case_index << "/" << m_totalcaseCount << ", group case: " << m_block_case_index << "/" << m_block_case_count << ", m_timeout_count:" << m_timeout_count);
        	if (m_exeType != "auto") {
        		if(g_show_log)
        			DBG_ONLY("manual cases. please check on device." << endl);
        	}
		}
	} else if (prequest->path == "/shut_down_server") {
		if (g_run_wiget == true)
			killAllWidget(); // kill all widget when shutdown server
		gIsRun = 0;
	} else if (prequest->path.find("/init_session_id") != string::npos) {// invoke by index.html to record a session id
		int index = prequest->path.find('=');
		if (index != -1) {
			m_running_session = prequest->path.substr(index + 1);
			if(g_show_log)
				DBG_ONLY("[ sessionID: " << m_running_session << " is gotten from the client ]");
		} else {
			if(g_show_log)
				DBG_ONLY("[ invalid session id ]");
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
				json_str = m_test_cases[m_block_case_index].to_json().toStyledString();
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
			set_timer(60); // check every 60 seconds to make sure widget alive when run manual cases.
		}
	} else if (prequest->path.find("/commit_manual_result") != string::npos) {// invoke by index.html to provide result of a manual case.
		if ((prequest->content.length() == 0) || (!m_test_cases)) {
			json_str = "{\"Error\":\"no manual result\"}";
		} else {
			find_id(splitContent(prequest->content), false); // will set index in find_id
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
	//generate_xml:from index_html, a maually block finished when click done in widget, or no manual cases anymore(when run auto block)
	else if (prequest->path == "/generate_xml") {
       	if (m_exeType != "auto") {
			cancel_time_check();
			m_block_finished = true;
			if (m_current_block_index == m_totalBlocks)
				m_set_finished = true;
		}
	}
	//from com module,when m_set_finished is true
	else if (prequest->path == "/get_test_result") {
		cancel_time_check();
		if (!m_test_cases) {
			json_str = "{\"Error\":\"no case\"}";
		} else {
			Json::Value root;
			root["count"] = "0";
			root["cases"] = m_result;// m_result will always be empty here
			json_str = root.toStyledString();
		}
	}
    // index.html invoke this with id and result of an auto case, auto case commit result. 
    // we need find correct test case by id, and record test result to it.
    else if (prequest->path == "/commit_result") {
		if ((prequest->content.length() == 0) || (!m_test_cases)) {
			json_str = "{\"Error\":\"no result\"}";
			m_last_auto_result = "BLOCK";
		} else {
			Json::Value paras = splitContent(prequest->content);
			if (m_running_session == paras["session_id"].asString()) {
	            find_id(paras, true);
            }
        }
    } else if (prequest->path == "/set_capability") {// by com-module to send capability data
        Json::Reader reader;

        reader.parse(prequest->content, m_capability);
    } else if (prequest->path.find("/capability") != string::npos) {// by test suite. only one query parameter each time
        json_str = "{\"support\":0}";

		Json::Value paras = splitContent(prequest->content);
        string value = paras["value"].asString();
        string name = paras["name"].asString();
		for (unsigned int i = 0; i < name.size(); i++)
			name[i] = tolower(name[i]);

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
		json_str = "{\"Error\":\"unknown request\"}";
	}

	if (json_str != "")
		sendresponse(s, 200, prequest, json_str);

#if defined(__WIN32__) || defined(__WIN64__)
	cout << "server response is:" << json_str << endl;
#else
    if(g_show_log)
    	DBG_ONLY("server response is:" << json_str);
#endif
}

// find correct case according the id sent by widget
void HttpServer::find_id(Json::Value paras, bool auto_test) {
	string id = paras["case_id"].asString();

	bool found = false;
	for (int i = 0; i < m_block_case_count; i++) {
		if (m_test_cases[i].case_id == id) {
			m_test_cases[i].set_result(paras);
			found = true;
			if (!auto_test) // should auto test use this logic?
				m_block_case_index = i; // set index by id found
			print_info_string(i);
			break;
		}
	}
	if (!found) {
		if(g_show_log)
			DBG_ONLY("[ Error: can't find any test case by id: " << id << " ]");
	}

	if (auto_test)
		m_last_auto_result = paras["result"].asString();
}

// create new thread for each http request
void* processthread(void *para) {
	string recvstr = "";
	char *buffer = new char[MAX_BUF]; // suppose 1 case need 1k, 100 cases will be sent each time, we need 100k memory?
	memset(buffer, 0, MAX_BUF);
	long iDataNum = 0;
	int recvnum = 0;
	thread_info *tinfo = (thread_info *) para;
	struct HttpRequest httprequest;
	httprequest.content = "";
	httprequest.path = "";

	int content_length = 0;
	int real_content_length = 0;
	while (1) {
		iDataNum = recv(tinfo->clientsocket, buffer + recvnum,
				MAX_BUF - recvnum - 1, 0);
		if (iDataNum <= 0) {
			delete[] buffer;
			close(tinfo->clientsocket);
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
							if(g_show_log)
								DBG_ONLY("[ can't find \\r\\n\\r\\n or \\n\\n ]");
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
	switch (tinfo->server->getrequest(recvstr, &httprequest)) {
	case GET:
	case POST:
		tinfo->server->processpost(tinfo->clientsocket, &httprequest);
		break;
	default:
		break;
	}
	close(tinfo->clientsocket);
	pthread_exit (NULL);
	return 0;
}

void timer_handler(int signum) {
	DBG_ONLY("receive signum = " << signum);
	httpserver.timeout_action();
}

void HttpServer::timeout_action() {
	string json_str = "{\"OK\":1}";
	if (!m_server_checked) {// start widget again in case it dead. browser not need to restart
		g_error_code = 1;
		/*if (m_check_times < 3) {
			m_check_times++;
			if(g_show_log)
				DBG_ONLY("[ checking the client " << m_check_times << " times ]");
			if (g_run_wiget == true) start_client();
			set_timer(20);
		} else {// widget fail for 3 times. finish current set.
			m_set_finished = true;
			m_block_finished = true;
		}*/
	} else if (!m_test_cases) {
		json_str = "{\"Error\":\"no case\"}";
	} else if (m_exeType != "auto") {// skip to next set if widget crash when run manual cases
		if (g_run_wiget == true) {// check whether widget is running
			string cmd = "ps ax | grep " + m_suite_id + " | grep -v grep | awk '{print $NF}'";
			if (!run_cmd(cmd, m_suite_id, NULL)) {
				m_set_finished = true;
				m_block_finished = true;
			}
			else set_timer(60); // continue to set timer for manual cases
		}
	} else if (m_block_case_index < m_block_case_count) {
		g_error_code = 3;
		checkResult(&m_test_cases[m_block_case_index]);
		json_str = "{\"OK\":\"send timeout\"}";
	} else {
		json_str = "{\"Error\":\"case out of index\"}";
	}
	if (g_show_log) DBG_ONLY(json_str);
}

void HttpServer::print_info_string(int case_index) {
	if(g_show_log){
		DBG_ONLY(endl << "execute case: ");
		DBG_ONLY(m_suite_name << " # " << m_test_cases[case_index].case_id << " ..(" << m_test_cases[case_index].result << ")");

		if (m_test_cases[case_index].result != "PASS") // print error message if case not pass
			DBG_ONLY(m_test_cases[case_index].std_out);
	}

    pthread_mutex_lock( &result_mutex );
    m_result.append(m_test_cases[case_index].result_to_json());
    if (m_exeType == "auto") {
		if (m_block_case_index < m_block_case_count) {
			m_block_case_index++;
			m_total_case_index++;
		}
	}
    pthread_mutex_unlock( &result_mutex );
}

// prepare to run current auto case by set the start time, etc.
bool HttpServer::get_auto_case(string content, string *type) {
	if (!m_killing_widget) {
		if (content != "") {
			string value = content.substr(content.find("=") + 1);
			if (value.length() > 0) {
				if (m_running_session == value) {
					if (m_block_case_index < m_block_case_count) {
						set_timer(m_test_cases[m_block_case_index].timeout_value+10); // +10 is to avoid dup result from suite if case timeout
						m_test_cases[m_block_case_index].set_start_at();
						if(g_show_log)
							DBG_ONLY(endl << "start time: " << m_test_cases[m_block_case_index].start_at);
						return true;
					} else {
						if(g_show_log)
							DBG_ONLY(endl << "[ no auto case is available any more ]");
						*type = "none";
						m_block_finished = true;
						if (m_current_block_index == m_totalBlocks)
							m_set_finished = true; // the set is finished if current block is the last block
					}
				} else {
					if(g_show_log)
						DBG_ONLY("[ Error: invalid session ID ]");
					*type = "invalid";
				}
			}
		}
	} else {
		if(g_show_log)
			DBG_ONLY(endl << "[ restart client process is activated, exit current client ]");
		*type = "stop";
	}
	return false;
}

void* socket_thread(void *para) {
#if defined(__WIN32__) || defined(__WIN64__)
#else
	sigset_t set;
	int s;

	/* Block SIGALRM; other threads created by main() will inherit
	  a copy of the signal mask. */

	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	s = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if ((s != 0) && (g_show_log))
		DBG_ONLY("fail to pthread_sigmask !!!");
#endif

	struct sockaddr_in clientAddr;
	int addr_len = sizeof(clientAddr);

	while (httpserver.gIsRun) {
#if defined(__WIN32__) || defined(__WIN64__)
		int clientsocket = accept(serversocket, (struct sockaddr *) &clientAddr,
				(int*) &addr_len);
#else
		int clientsocket = accept(serversocket, (struct sockaddr *) &clientAddr,
				(socklen_t*) &addr_len);
#endif
		if (clientsocket < 0) {
			if(g_show_log)
				DBG_ONLY("fail to accept client socket !!!");
		} else {
			thread_info tinfo;
			tinfo.server = &httpserver;
			tinfo.clientsocket = clientsocket;
			if(g_show_log)
				DBG_ONLY("open " << clientsocket);
			pthread_create(&tinfo.thread_id, NULL, processthread, &tinfo);
			//if (tinfo.thread_id != 0) pthread_detach(tinfo.thread_id);// this will release resource after threat exit. otherwise it may not execute the thread after run for a while. but still cause some strange issue
#if defined(__WIN32__) || defined(__WIN64__)
			if (tinfo.thread_id.p)
				pthread_join(tinfo.thread_id, NULL);
			else if(g_show_log)
				DBG_ONLY("fail to create thread !!!");
#else
			if (tinfo.thread_id != 0)
				pthread_join(tinfo.thread_id, NULL);
			else if(g_show_log)
				DBG_ONLY("fail to create thread !!!");
#endif
		}
	}
	close(serversocket);
	pthread_exit (NULL);
	return 0;
}

//start the socket server, listen to client
void HttpServer::StartUp() {
	if(g_show_log){
		outputFile.open("httpserver_log.txt", ios::out);
		if ( (outputFile.rdstate() & std::ifstream::failbit ) != 0 ) {
			cout << "Error opening 'httpserver_log.txt'" << endl;
			g_openlog_fail = true;
		}
		DBG_ONLY("httpserver.g_port is:" << g_port);
	}

#if defined(__WIN32__) || defined(__WIN64__)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		cout << "fail to init socket !!!" << endl;
		if (g_show_log)
			DBG_ONLY("fail to init socket !!!");
		return;
	}
#endif

	if ((serversocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("fail to create server socket !!!");
		if(g_show_log)
			DBG_ONLY("fail to create server socket !!!");
		return;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(g_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int tmp = 1;
#if defined(__WIN32__) || defined(__WIN64__)
	if (setsockopt(serversocket, SOL_SOCKET, SO_REUSEADDR, (char*) &tmp, sizeof(int)) < 0);
#else
	if (setsockopt(serversocket, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0)
#endif
		if(g_show_log) DBG_ONLY("fail to set SO_REUSEADDR !!!");

	struct linger so_linger;
	so_linger.l_onoff = true;
	so_linger.l_linger = 10; // wait 10 seconds to close, to avoid TIME_WAIT of client
#if defined(__WIN32__) || defined(__WIN64__)
	if (setsockopt(serversocket, SOL_SOCKET, SO_LINGER, (char*) &so_linger, sizeof(so_linger)) < 0)
#else
	if (setsockopt(serversocket, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0)
#endif	
		if(g_show_log) DBG_ONLY("fail to set SO_LINGER !!!");

	if (bind(serversocket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("fail to bind address !!!");
		if(g_show_log) DBG_ONLY("fail to bind address !!!");
		return;
	}

	if (listen(serversocket, 5) < 0) {
		perror("fail to listen !!!");
		if(g_show_log) DBG_ONLY("fail to listen !!!");
		return;
	}
	gIsRun = 1;
	cout << "[ Server is running, bind to " << g_port << " .....]" << endl;
	if(g_show_log)
		DBG_ONLY("[ Server is running, bind to " << g_port << " .....]");

#if defined(__WIN32__) || defined(__WIN64__)
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, socket_thread, NULL);
	if (thread_id.p) pthread_join(thread_id, NULL);
	WSACleanup();
#else
	daemon(0,0);
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, socket_thread, NULL);
	if (thread_id != 0) pthread_join(thread_id, NULL);
#endif
}

// set result to block for current case if timeout
void HttpServer::checkResult(TestCase* testcase) {
	Json::Value result;
	result["result"] = "BLOCK";
	result["msg"] = "Time is out";
	testcase->set_result(result);
	m_last_auto_result = "BLOCK";

	print_info_string(m_block_case_index);

	m_timeout_count++;

	if(g_show_log) DBG_ONLY("[ start new client now ]");
	//start_client(); // start widget again in case it dead. browser not need to restart
}

// try to kill all widget listed by wrt-launch -l
void HttpServer::killAllWidget() {
	m_killing_widget = true;
	string cmd = "killall " + g_launcher + " 2>/dev/null";
	system(cmd.c_str());

	std::vector<string> widgets;
	cmd = g_launcher+" -l | awk '{print $NF}' | sed -n '3,$p'";
	run_cmd(cmd, "", &widgets);

	std::vector<string> processes;
	cmd = "ps ax | awk '{print $NF}' | sed -n '2,$p'";
	run_cmd(cmd, "", &processes);

	for (unsigned int i = 0; i < widgets.size(); i++) 
		for (unsigned int j = 0; j < processes.size(); j++)
			if (string::npos != processes[j].find(widgets[i])) {
				string cmd = g_launcher +" -k " + widgets[i]; // use wrt-launcher to kill widget
				run_cmd(cmd, "result: killed", NULL);
				break;
			}
	m_killing_widget = false;
}

// try to start widget
/*void HttpServer::start_client() {
	if (m_set_finished) return;// not restart widget if set finished
	if(g_run_wiget == true){
	    run_cmd(g_kill_cmd, "result: killed", NULL);// kill before launch incase sometime relaunch fail, widget hang
		sleep(2);
		while (!run_cmd(g_launch_cmd, "result: launched", NULL)) {
			m_failto_launch++;
			if(g_show_log)
				DBG_ONLY("m_failto_launch: " << m_failto_launch);
			if (m_failto_launch >= m_max_fail_launch) {
	            m_set_finished = true;
	            m_block_finished = true;
	            m_total_case_index = m_totalcaseCount;
				break;
			}
	        run_cmd(g_kill_cmd, "result: killed", NULL);
			sleep(5); // try until start widget success
		}
		m_failto_launch = 0;
	}
	else{
		if (g_launcher == "") return;// do nothing if comm-module send an empty g_launcher

		if (g_kill_cmd != "") run_cmd(g_kill_cmd, "", NULL);
		sleep(5); // sleep 5 seconds to avoid launch browser too frequently, otherwise may launch fail.
		int status = system(g_launch_cmd.c_str()); // can't use popen here for it will not return until cmd finish
		if ((status != 0) && g_show_log)
			DBG_ONLY("[cmd: " << g_launch_cmd << " error]");
	}
}*/

// run shell cmd. return true if the output equal to expectString. show cmd and output if showcmdAnyway.
bool HttpServer::run_cmd(string cmdString, string expectString, std::vector<string> *output) {
	bool ret = false;

	char buf[128];
	memset(buf, 0, 128);
	FILE *pp;
	if ((pp = popen(cmdString.c_str(), "r")) == NULL) {
		if(g_show_log) DBG_ONLY("popen() error!");
		return ret;
	}
	if(g_show_log) DBG_ONLY(cmdString);
	while (fgets(buf, sizeof buf, pp)) {
		if(g_show_log) DBG_ONLY(buf);
		buf[strlen(buf) - 1] = 0; // remove the character return at the end.
		if (strstr(buf, expectString.c_str())) ret = true;
		if (output) output->push_back(buf);
		memset(buf, 0, 128);		
	}
	pclose(pp);	
	return ret;
}
