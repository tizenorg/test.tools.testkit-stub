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
	//void start_client();
	bool run_cmd(string cmdString, string expectString, std::vector<string> *output);
	void print_info_string(int case_index);
	void find_id(Json::Value paras, bool auto_test);
	void getCurrentTime();
	void cancel_time_check();
	void set_timer(int timeout_value);
	Json::Value splitContent(string content);
	void timeout_action();

#if defined(__WIN32__) || defined(__WIN64__)
#else	
	struct sigaction sa;
	struct itimerval timer;
#endif
	int gIsRun;

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
	Json::Value m_result;

	int m_killing_widget;
	
	string m_running_session;

	string m_last_auto_result;

	int m_failto_launch; // time of fail to launch
	int m_max_fail_launch;

	//some variables get from cmd line
	int g_port;
	string g_launch_cmd;
	string g_kill_cmd;
	string g_launcher;//lancher name:wrt-launcher/browser
	string m_suite_name;
	string m_suite_id;
	bool g_run_wiget;//whether run on the device with wiget
};
