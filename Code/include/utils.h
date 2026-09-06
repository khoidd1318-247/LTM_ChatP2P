#pragma once
#include <string>

using namespace std;

string get_current_time_str();
string generate_random_id();
string generate_msg_id();
bool isValidIPv4(const string &ip);
bool isValidPort(const string &portStr, int &outPort);