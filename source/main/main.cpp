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

#include <stdlib.h>
#include <string>
#include <json/json.h>

#include "httpserver.h"
#if defined(__WIN32__) || defined(__WIN64__)
#include <windows.h>
#endif
bool g_show_log = false;
extern HttpServer httpserver;

//parse the cmd line parameters
void parse(int count, char *argv[], HttpServer *httpserver) {
	for (int i = 1; i < count; ++i) {
		string argvstr = argv[i];
		if (argvstr == "--debug"){
			g_show_log = true;
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
	if (argc > 1) {
		parse(argc, argv, &httpserver);
	}
	httpserver.StartUp();
	return 0;
}

