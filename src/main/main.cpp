#include <string>
#include <json/json.h>

#include "httpserver.h"

int gServerStatus = 0;
int gIsRun = 0;

//parse the cmd line parameters
void parse(int count, char *argv[], HttpServer *httpserver) {
	for (int i = 1; i < count; ++i) {
		string argvstr = argv[i];
		if (argvstr == "--debug"){
			httpserver->g_show_log = true;
			continue;
		}
		int sepindex = argvstr.find(":");
		if (sepindex > -1) {
			string key = argvstr.substr(0, sepindex);
			string value = argvstr.substr(sepindex + 1);
			if (key == "--port") {
				httpserver->g_port = value;
			} else if (key == "--hidestatus") {
				httpserver->g_hide_status = value;
			} else if (key == "--testsuite") {
				httpserver->g_test_suite = value;
			} else if (key == "--exe_sequence") {
				httpserver->g_exe_sequence = value;
			} else if (key == "--enable_memory_collection") {
				httpserver->g_enable_memory_collection = value;
			} else if (key == "--external-test") {
				//parse the value to lancher and index_file
				httpserver->g_launcher = value;
				if (value == "wrt-launcher")
				{
					httpserver->g_run_wiget = true;
					httpserver->g_launch_cmd = httpserver->g_launcher + " -s " + httpserver->g_test_suite;
					httpserver->g_kill_cmd = httpserver->g_launcher + " -k " + httpserver->g_test_suite;
				}
				else
					httpserver->g_run_wiget = false;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	HttpServer httpserver;
	if (argc > 1) {
		parse(argc, argv, &httpserver);
	}
	httpserver.StartUp();
	return 0;
}

