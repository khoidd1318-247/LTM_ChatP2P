#include "P2PNode.h"
#include <iostream>

P2PNode::P2PNode() : listenSocket(INVALID_SOCKET), peerSocket(INVALID_SOCKET), isRunning(false) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

P2PNode::~P2PNode() {
    stop();
    WSACleanup();
}

void P2PNode::stop() {
    isRunning = false;
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    if (peerSocket != INVALID_SOCKET) {
        closesocket(peerSocket);
        peerSocket = INVALID_SOCKET;
    }
    if (listenThread.joinable()) listenThread.join();
    if (receiveThread.joinable()) receiveThread.join();
}

bool P2PNode::startListening(int port) {
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        return false;
    }

    isRunning = true;
    listenThread = std::thread(&P2PNode::listenLoop, this, port);
    return true;
}

void P2PNode::listenLoop(int port) {
    while (isRunning) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET incomingSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (incomingSocket != INVALID_SOCKET) {
            // In a true 1-on-1 P2P chat, we only keep one connection.
            // If already connected, reject.
            if (peerSocket != INVALID_SOCKET) {
                closesocket(incomingSocket);
                continue;
            }

            peerSocket = incomingSocket;
            if (onPeerConnected) onPeerConnected();
            
            receiveThread = std::thread(&P2PNode::receiveLoop, this);
        }
    }
}

bool P2PNode::connectToPeer(const std::string& ip, int port) {
    if (peerSocket != INVALID_SOCKET) return false; // Already connected

    peerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peerSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

    if (connect(peerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(peerSocket);
        peerSocket = INVALID_SOCKET;
        return false;
    }

    isRunning = true;
    if (onPeerConnected) onPeerConnected();
    
    receiveThread = std::thread(&P2PNode::receiveLoop, this);
    return true;
}

bool P2PNode::sendData(const std::string& data) {
    if (peerSocket == INVALID_SOCKET) return false;
    
    // Simple protocol: send length (4 bytes), then data
    int dataLen = data.length();
    int bytesSent = send(peerSocket, (char*)&dataLen, sizeof(dataLen), 0);
    if (bytesSent == SOCKET_ERROR) return false;
    
    bytesSent = send(peerSocket, data.c_str(), dataLen, 0);
    return bytesSent != SOCKET_ERROR;
}

void P2PNode::receiveLoop() {
    while (isRunning && peerSocket != INVALID_SOCKET) {
        int dataLen = 0;
        int bytesReceived = recv(peerSocket, (char*)&dataLen, sizeof(dataLen), 0);
        
        if (bytesReceived <= 0) {
            // Disconnected
            break;
        }

        std::string buffer;
        buffer.resize(dataLen);
        int totalReceived = 0;
        
        while (totalReceived < dataLen) {
            bytesReceived = recv(peerSocket, &buffer[totalReceived], dataLen - totalReceived, 0);
            if (bytesReceived <= 0) break;
            totalReceived += bytesReceived;
        }

        if (totalReceived == dataLen && onMessageReceived) {
            onMessageReceived(buffer);
        } else {
            break; // Error in stream
        }
    }

    closesocket(peerSocket);
    peerSocket = INVALID_SOCKET;
    if (onPeerDisconnected) onPeerDisconnected();
}
