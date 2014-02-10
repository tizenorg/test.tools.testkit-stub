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
#include <string.h>

#include <signal.h>
#include <sys/time.h>

#include <string>
using namespace std;

#include <json/json.h>

class TestCase {
public:
	TestCase();
	virtual ~TestCase();

public:
	string result; // "pass" or "fail", "block", "N/A"
	string start_at;
	string end_at;
	string std_out;

	// below m_case are sent from Com-module for each case.
	Json::Value m_case;
	string case_id;
	int timeout_value;

	char m_str_time[32]; // to store the time string
public:
	void init(const Json::Value value); // the case_node should be a string in json format
	Json::Value to_json();
	Json::Value result_to_json();
	void set_result(Json::Value paras);
	void set_start_at();
	void getCurrentTime();
};
