#define ASIO_STANDALONE
#include <asio.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;
using asio::ip::tcp;

// --- GLOBAL VARIABLES & STATE ---
mutex chat_mutex; 
vector<string> chatHistory;
char username[128] = ""; // Default username is now empty
char targetIP[128] = "127.0.0.1";
char portBuf[16] = "8080";
char messageBuf[1024] = "";

enum AppState { IDLE, WAITING_FOR_PEER, CONNECTED };
AppState currentState = IDLE;

// Asio network objects
asio::io_context* io_context_ptr = nullptr;
tcp::socket* peer_socket = nullptr;
tcp::acceptor* peer_acceptor = nullptr;
thread* network_thread = nullptr;
asio::streambuf read_buffer;

// Function to add messages to the UI
void add_log(const string& msg) {
    lock_guard<mutex> lock(chat_mutex);
    chatHistory.push_back(msg);
}

// Function to clean up and disconnect
void stop_network() {
    if (io_context_ptr) io_context_ptr->stop();
    if (peer_socket) { asio::error_code ec; peer_socket->close(ec); delete peer_socket; peer_socket = nullptr; }
    if (peer_acceptor) { asio::error_code ec; peer_acceptor->close(ec); delete peer_acceptor; peer_acceptor = nullptr; }
    if (network_thread && network_thread->joinable()) { network_thread->join(); delete network_thread; network_thread = nullptr; }
    if (io_context_ptr) { delete io_context_ptr; io_context_ptr = nullptr; }
    currentState = IDLE;
}

// Asynchronous read loop for incoming messages
void async_read_loop() {
    asio::async_read_until(*peer_socket, read_buffer, '\n', [](const asio::error_code& error, size_t bytes) {
        if (!error) {
            istream is(&read_buffer);
            string line;
            getline(is, line);
            if (!line.empty()) add_log(line);
            async_read_loop(); // Continue listening
        } else {
            add_log("[System] Connection lost.");
            currentState = IDLE;
        }
    });
}

// HOST MODE: Listen for incoming connections
void start_hosting(int port) {
    stop_network();
    chatHistory.clear();
    try {
        io_context_ptr = new asio::io_context();
        peer_acceptor = new tcp::acceptor(*io_context_ptr, tcp::endpoint(tcp::v4(), port));
        peer_socket = new tcp::socket(*io_context_ptr);
        
        currentState = WAITING_FOR_PEER;
        add_log("[System] Opening Port " + to_string(port) + " waiting for peer...");

        peer_acceptor->async_accept(*peer_socket, [](const asio::error_code& error) {
            if (!error) {
                add_log("[System] Peer connected! Start chatting.");
                currentState = CONNECTED;
                async_read_loop();
            } else {
                add_log("[System] Host error: " + error.message());
                currentState = IDLE;
            }
        });

        network_thread = new thread([]() { io_context_ptr->run(); });
    } catch(exception& e) {
        add_log(string("[System] Error: ") + e.what());
        currentState = IDLE;
    }
}

// JOIN MODE: Connect to a remote IP
void start_connecting(string ip, int port) {
    stop_network();
    chatHistory.clear();
    try {
        io_context_ptr = new asio::io_context();
        peer_socket = new tcp::socket(*io_context_ptr);
        
        currentState = WAITING_FOR_PEER;
        add_log("[System] Connecting to " + ip + ":" + to_string(port) + "...");

        tcp::endpoint endpoint(asio::ip::make_address(ip), port);
        peer_socket->async_connect(endpoint, [](const asio::error_code& error) {
            if (!error) {
                add_log("[System] Connected successfully! Start chatting.");
                currentState = CONNECTED;
                async_read_loop();
            } else {
                add_log("[System] Connection failed: " + error.message());
                currentState = IDLE;
            }
        });

        network_thread = new thread([]() { io_context_ptr->run(); });
    } catch(exception& e) {
        add_log(string("[System] Error: ") + e.what());
        currentState = IDLE;
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(600, 700, "P2P Chat App (True P2P)", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr; 
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        // UI for IDLE state (selecting host or join)
        if (currentState == IDLE) {
            ImGui::Text("=== TRUE P2P CHAT APP ===");
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::Text("UserName:");
            ImGui::InputText("##username", username, IM_ARRAYSIZE(username));
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("--- OPTION 1: HOST A ROOM ---");
            ImGui::Text("Port:");
            ImGui::InputText("##hostport", portBuf, IM_ARRAYSIZE(portBuf));
            if (ImGui::Button("Host Room", ImVec2(200, 40))) {
                start_hosting(atoi(portBuf));
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("--- OPTION 2: JOIN A ROOM ---");
            ImGui::Text("Peer IP Address:");
            ImGui::InputText("##targetip", targetIP, IM_ARRAYSIZE(targetIP));
            if (ImGui::Button("Join Room", ImVec2(200, 40))) {
                start_connecting(targetIP, atoi(portBuf));
            }
        } 
        // UI for Connecting/Connected state
        else {
            ImGui::Text("Hello, %s! (P2P Mode)", username);
            ImGui::SameLine(ImGui::GetWindowWidth() - 170);
            if (ImGui::Button("Disconnect / Cancel")) {
                stop_network();
            }
            ImGui::Separator();

            ImGui::BeginChild("ChatRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 10), true);
            {
                lock_guard<mutex> lock(chat_mutex);
                for (const auto& msg : chatHistory) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (currentState == CONNECTED) {
                ImGui::PushItemWidth(-70);
                bool isEnterPressed = ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                
                // Logic to send messages directly over Asio TCP Socket
                if (ImGui::Button("Send", ImVec2(60, 0)) || isEnterPressed) {
                    if (strlen(messageBuf) > 0) {
                        string display_name = (strlen(username) > 0) ? string(username) : "Anonymous";
                        string out_msg = display_name + ": " + messageBuf + "\n";
                        asio::error_code ec;
                        asio::write(*peer_socket, asio::buffer(out_msg), ec);
                        
                        if (!ec) {
                            add_log(display_name + ": " + messageBuf);
                            messageBuf[0] = '\0';
                            ImGui::SetKeyboardFocusHere(-1); 
                        } else {
                            add_log("[System] Message send error: " + ec.message());
                        }
                    }
                }
            }
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    stop_network(); 
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}