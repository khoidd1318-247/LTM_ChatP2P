#include <iostream>  
#include <winsock2.h>  
#pragma comment(lib, \" "ws2_32.lib\)  
int main() { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); SOCKET s = socket(AF_INET, SOCK_STREAM, 0); sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(8080); if(bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) std::cout << \Bind" failed: "\ << WSAGetLastError() << std::endl; else std::cout << \Bind" "success\ << std::endl; return 0; }  
