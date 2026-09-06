#include "utils.h"
#include "globals.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

using namespace std;

string get_current_time_str() {
  auto now = chrono::system_clock::now();
  auto in_time_t = chrono::system_clock::to_time_t(now);
  tm buf;
#if defined(_WIN32)
  localtime_s(&buf, &in_time_t);
#else
  localtime_r(&in_time_t, &buf);
#endif
  ostringstream ss;
  ss << setfill('0') << setw(2) << buf.tm_hour << ":" << setw(2) << buf.tm_min
     << ":" << setw(2) << buf.tm_sec;
  return ss.str();
}

string generate_random_id() { return "#" + to_string(1000 + (rand() % 9000)); }

string generate_msg_id() {
  messageCounter++;
  return local_user_id + "_" + to_string(messageCounter);
}

bool isValidIPv4(const string &ip) {
  if (ip == "localhost" || ip == "127.0.0.1")
    return true;
  stringstream ss(ip);
  string segment;
  vector<string> segments;
  while (getline(ss, segment, '.'))
    segments.push_back(segment);
  if (segments.size() != 4)
    return false;
  for (const string &seg : segments) {
    if (seg.empty() || seg.size() > 3)
      return false;
    for (char c : seg)
      if (!isdigit(static_cast<unsigned char>(c)))
        return false;
    int num = stoi(seg);
    if (num < 0 || num > 255)
      return false;
  }
  return true;
}

bool isValidPort(const string &portStr, int &outPort) {
  if (portStr.empty())
    return false;
  for (char c : portStr)
    if (!isdigit(static_cast<unsigned char>(c)))
      return false;
  try {
    int p = stoi(portStr);
    if (p >= 1 && p <= 65535) {
      outPort = p;
      return true;
    }
  } catch (...) {
  }
  return false;
}