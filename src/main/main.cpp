#include <stdlib.h>
#include <string>
#include <json/json.h>

#include "httpserver.h"

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
				httpserver->g_port = atoi(value.c_str());
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

