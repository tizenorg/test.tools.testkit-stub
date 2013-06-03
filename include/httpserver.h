#include <stdio.h>
#include <fstream>
#include <string.h>
#include "testcase.h"

#include <string>
using namespace std;

void timer_handler(int signum);

typedef struct HttpRequest {
	string method;     // request type
	string path;       // request path
	string content;    // request content
	int contentlength; // length of request length
	int contenttype;   // response content type
	string prefix;
	int responsecode;  //response code
} HR;

class HttpServer {
public:
	HttpServer();
	virtual ~HttpServer();
public:
	void StartUp();
	void processpost(int s, struct HttpRequest *prequest);
	void sendresponse(int s, int code, struct HttpRequest *prequest,
			string content);
	int sendsegment(int s, string buffer);
	void parse_json_str(string jsonstr);
	int getrequest(string requestbuf, struct HttpRequest *prequest);
	bool get_auto_case(string content, string *type);
	void checkResult(TestCase* testcase);
	void killAllWidget();
	void start_client();
	bool run_cmd(string cmdString, string expectString, bool showcmdAnyway);
	void print_info_string(int case_index);
	void find_purpose(Json::Value paras, bool auto_test);
	void getCurrentTime();
	void cancel_time_check();
	void set_timer(int timeout_value);
	void getAllWidget();
	Json::Value splitContent(string content);

	struct sigaction sa;
	struct itimerval timer;
	int gIsRun;
	int clientsocket;

	int gServerStatus;

	string m_exeType; //auto;manual	
	TestCase *m_test_cases; //the case array

	//block 
	int m_totalBlocks;
	int m_current_block_index;
	int m_totalcaseCount;
	int m_total_case_index; //current case index in set
	int m_block_case_index; //current case index in block
	int m_block_case_count; //case count in block
	bool m_block_finished;
	bool m_set_finished;
	bool m_server_checked;
	int m_check_times;

	Json::Value m_capability;
	//TestStatus   
	int m_timeout_count; // continusously time out count

	int m_killing_widget;
	
	std::vector<string> m_widgets; // store all the widgets short name

	string m_running_session;

	string m_last_auto_result;

	int m_failto_launch; // time of fail to launch

	bool m_rerun; // true when re-run a case

	//some variables get from cmd line
	bool g_show_log;
	int g_port;
	string g_launch_cmd;
	string g_kill_cmd;
	string g_launcher;//lancher name:wrt-launcher/browser
	string m_suite_name;
	bool g_run_wiget;//whether run on the device with wiget

	ofstream outputFile;
};
