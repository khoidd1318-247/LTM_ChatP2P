#define ASIO_STANDALONE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <algorithm>

#include "globals.h"
#include "utils.h"
#include "network.h"
#include "ui_helpers.h"

namespace fs = std::filesystem;
using namespace std;

int main() {
  srand(static_cast<unsigned int>(time(nullptr)));
  local_user_id = generate_random_id();

  if (!glfwInit())
    return -1;
  GLFWwindow *window = glfwCreateWindow(760, 860, "P2P Chat", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.IniFilename = nullptr;

  apply_modern_theme();

  ImFontConfig font_config;
  font_config.OversampleH = 2;
  font_config.OversampleV = 2;
  if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &font_config, io.Fonts->GetGlyphRangesVietnamese());
  } else if (fs::exists("C:\\Windows\\Fonts\\arial.ttf")) {
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 17.0f, &font_config, io.Fonts->GetGlyphRangesVietnamese());
  } else {
    io.Fonts->AddFontDefault();
  }

  if (fs::exists("C:\\Windows\\Fonts\\seguiemj.ttf")) {
    ImFontConfig emoji_config;
    emoji_config.MergeMode = true;
    emoji_config.OversampleH = 1;
    emoji_config.OversampleV = 1;
    static const ImWchar emoji_ranges[] = {
        0x2000, 0x3300,
        0x2600, 0x27FF,
        0xE000, 0xFFFD,
        0};
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 17.0f, &emoji_config, emoji_ranges);
  }

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  glfwSetCharCallback(window, CustomDebouncedCharCallback);
  ImGui_ImplOpenGL3_Init("#version 130");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (remote_is_typing) {
      auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - last_remote_typing_time).count();
      if (elapsed > 3) remote_is_typing = false;
    }

    if (currentState == IDLE) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.48f, 0.60f, 1.00f, 1.00f), "P2P Chat");
      ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "Direct Peer-to-Peer Messaging");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (!statusErrorMessage.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.12f, 0.15f, 1.00f));
        ImGui::BeginChild("ErrorBanner", ImVec2(0, 60), true);
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "System Alert");
        ImGui::TextWrapped("%s", statusErrorMessage.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
      }

      ImGui::Text("Display Name");
      ImGui::InputText("##username", username, IM_ARRAYSIZE(username));
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.40f, 0.80f, 0.50f, 1.00f), "%s", local_user_id.c_str());
      ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "Unique session identifier for this peer");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      int parsedPort = 8080;
      bool validPort = isValidPort(portBuf, parsedPort);
      bool validIP = isValidIPv4(targetIP);

      ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Option 1: Host Room");
      ImGui::Text("Port (Digits 0-9 only)");
      ImGui::InputText("##hostport", portBuf, IM_ARRAYSIZE(portBuf), ImGuiInputTextFlags_CallbackCharFilter, PortInputFilter);

      if (!validPort) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid Port! Must be between 1 and 65535");
      else ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Valid Port (%d)", parsedPort);

      ImGui::BeginDisabled(!validPort);
      if (ImGui::Button("Host Room", ImVec2(200, 40))) start_hosting(parsedPort);
      ImGui::EndDisabled();

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Option 2: Join Room");
      ImGui::Text("Peer IP Address (Digits and '.' only)");
      ImGui::InputText("##targetip", targetIP, IM_ARRAYSIZE(targetIP), ImGuiInputTextFlags_CallbackCharFilter, IPInputFilter);
      
      ImGui::Text("Target Port (Digits 0-9 only)");
      ImGui::InputText("##joinport", joinPortBuf, IM_ARRAYSIZE(joinPortBuf), ImGuiInputTextFlags_CallbackCharFilter, PortInputFilter);

      int parsedJoinPort = 8080;
      bool validJoinPort = isValidPort(joinPortBuf, parsedJoinPort);

      if (!validIP) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid IP format (Standard format: x.x.x.x)");
      else if (!validJoinPort) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid Port! Must be between 1 and 65535");
      else ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Valid IP & Port format");

      ImGui::BeginDisabled(!validJoinPort || !validIP);
      if (ImGui::Button("Join Room", ImVec2(200, 40))) start_connecting(targetIP, parsedJoinPort);
      ImGui::EndDisabled();
    } else {
      string my_name = (strlen(username) > 0) ? string(username) : "Anonymous";

      ImGui::BeginChild("HeaderBar", ImVec2(0, 56), true);
      {
        render_avatar(my_name, local_user_id, 32.0f);
        ImGui::SameLine();

        if (currentState == CONNECTED) ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "Connected");
        else if (currentState == WAITING_FOR_PEER) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Waiting for Peer...");
        else ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Disconnected");

        ImGui::SameLine();
        ImGui::Text(" | You: %s %s • %s", my_name.c_str(), local_user_id.c_str(), local_role.c_str());

        if (currentState == CONNECTED && !peer_username.empty()) {
          ImGui::SameLine();
          ImGui::Text(" | Peer:");
          ImGui::SameLine();
          render_avatar(peer_username, peer_user_id, 24.0f);
          ImGui::SameLine();
          ImGui::Text("%s %s", peer_username.c_str(), peer_user_id.c_str());
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 240);
        if (ImGui::Button(showInfoPanel ? "Hide Info" : "Room Info", ImVec2(85, 30))) showInfoPanel = !showInfoPanel;

        ImGui::SameLine();
        if (ImGui::Button("Leave Room", ImVec2(120, 30))) {
          send_raw_line("[LEAVE]|" + local_user_id + "|" + my_name);
          stop_network();
        }
      }
      ImGui::EndChild();

      if (showInfoPanel) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.16f, 0.21f, 1.00f));
        ImGui::BeginChild("InfoPanel", ImVec2(0, 75), true);
        {
          ImGui::TextColored(ImVec4(0.48f, 0.60f, 1.00f, 1.00f), "Room Details & Connection Status");
          ImGui::Text("Protocol: Direct TCP P2P Socket  |  Port: %s", portBuf);
          if (local_role == "Peer") ImGui::Text("Host Target IP: %s  |  Peer ID: %s", targetIP, peer_user_id.empty() ? "Connecting..." : peer_user_id.c_str());
          else ImGui::Text("Hosting Port: %s  |  Connected Peer: %s", portBuf, peer_username.empty() ? "None" : (peer_username + " " + peer_user_id).c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
      }

      ImGui::PushItemWidth(220);
      ImGui::InputTextWithHint("##search", "Search messages...", searchBuf, IM_ARRAYSIZE(searchBuf));
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (ImGui::Button("Clear History")) {
        lock_guard<mutex> lock(chat_mutex);
        chatHistoryList.clear();
        unreadMessageCount = 0;
        unreadTrackingIndex = 0;
        editingMessageId = "";
      }

      float bottomPadding = (currentState == CONNECTED) ? 145.0f : 60.0f;
      ImGui::BeginChild("ChatRegion", ImVec2(0, -bottomPadding), true);
      {
        lock_guard<mutex> lock(chat_mutex);

        bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        if (chatHistoryList.size() > unreadTrackingIndex) {
          for (size_t i = unreadTrackingIndex; i < chatHistoryList.size(); i++) {
            const ChatMessage &incomingMsg = chatHistoryList[i];
            if (!incomingMsg.is_self && !incomingMsg.is_system && !wasAtBottom) unreadMessageCount++;
          }
          unreadTrackingIndex = chatHistoryList.size();
        }
        if (wasAtBottom) {
          unreadMessageCount = 0;
          send_pending_read_receipts_nolock();
        }

        string filterStr = searchBuf;
        transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        for (size_t idx = 0; idx < chatHistoryList.size(); idx++) {
          auto &msg = chatHistoryList[idx];

          if (!filterStr.empty()) {
            string contentLower = msg.content;
            transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);
            if (contentLower.find(filterStr) == string::npos) continue;
          }

          if (msg.is_system) {
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(msg.content.c_str()).x) * 0.5f);
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "%s • %s", msg.timestamp.c_str(), msg.content.c_str());
            ImGui::Spacing();
          } else {
            render_chat_bubble(idx, msg);
          }
        }
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() || scrollToBottomRequested) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomRequested = false;
        unreadMessageCount = 0;
        send_pending_read_receipts();
      }
      ImGui::EndChild();

      if (unreadMessageCount > 0) {
        string unreadLabel = to_string(unreadMessageCount) + " new message" + (unreadMessageCount > 1 ? "s" : "") + " ↓";
        float labelWidth = ImGui::CalcTextSize(unreadLabel.c_str()).x + 30.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - labelWidth) * 0.5f);
        if (ImGui::Button(unreadLabel.c_str())) scrollToBottomRequested = true;
      }

      if (currentState == CONNECTED) {
        if (remote_is_typing) {
          string typingName = peer_username.empty() ? "Peer" : peer_username;
          ImGui::TextColored(ImVec4(0.40f, 0.80f, 0.50f, 1.00f), "%s is typing...", typingName.c_str());
        }

        if (!replyTargetId.empty()) {
          ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.00f, 1.00f), "Replying to %s: \"%s\"", replyTargetName.c_str(), replyTargetText.c_str());
          ImGui::SameLine();
          if (ImGui::SmallButton("Cancel")) {
            replyTargetId = "";
            replyTargetName = "";
            replyTargetText = "";
          }
        }

        ImGui::Text("Quick Replies:");
        ImGui::SameLine();
        const char *quickReplies[] = {"Hello!", "I'm on my way!", "Sounds good!", "Call you later", "Got it!", "Thanks!"};
        for (int i = 0; i < 6; i++) {
          if (i > 0) ImGui::SameLine();
          if (ImGui::SmallButton(quickReplies[i])) strcpy_s(messageBuf, sizeof(messageBuf), quickReplies[i]);
        }

        ImGui::Text("Quick Emotes:");
        ImGui::SameLine();
        const char *emojis[] = {"❤️", "⭐", "✨", "✔", "✖"};
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        for (int i = 0; i < 5; i++) {
          if (i > 0) ImGui::SameLine();
          if (ImGui::Button(emojis[i], ImVec2(32, 26))) {
            if (strlen(messageBuf) + strlen(emojis[i]) < sizeof(messageBuf)) {
              strcat_s(messageBuf, sizeof(messageBuf), emojis[i]);
            }
          }
        }
        ImGui::PopStyleVar();

        if (ImGui::Button("Emoji", ImVec2(54, 0))) ImGui::OpenPopup("EmojiPickerPopup");
        
        if (ImGui::BeginPopup("EmojiPickerPopup")) {
          ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.00f, 1.0f), "Choose Emoji");
          ImGui::Separator();
          const char *palette[] = {
              "❤️", "⭐", "✨", "⚡", "☀️", "☁️", "☔", "☕",
              "✌️", "✋", "☝️", "✍️", "✉️", "✈️", "⌛", "⏰",
              "⚓", "⚙️", "⚠️", "⛔", "❌", "❓", "❗", "🎵",
              "⚽", "⚾", "⛄", "⛅", "✔", "✖", "✳", "❇"};
          int totalEmojis = IM_ARRAYSIZE(palette);
          int cols = 8;
          for (int eIdx = 0; eIdx < totalEmojis; eIdx++) {
            if (eIdx % cols != 0) ImGui::SameLine();
            ImGui::PushID(eIdx);
            if (ImGui::Button(palette[eIdx], ImVec2(32, 28))) {
              if (strlen(messageBuf) + strlen(palette[eIdx]) < sizeof(messageBuf)) {
                strcat_s(messageBuf, sizeof(messageBuf), palette[eIdx]);
              }
            }
            ImGui::PopID();
          }
          ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(-70);
        bool isInputChanged = ImGui::InputText("##InputBox", messageBuf, IM_ARRAYSIZE(messageBuf), ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsItemActive()) {
          auto now = chrono::steady_clock::now();
          if (chrono::duration_cast<chrono::milliseconds>(now - last_local_typing_sent).count() > 800) {
            send_typing_status(strlen(messageBuf) > 0);
            last_local_typing_sent = now;
          }
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Send", ImVec2(60, 0)) || isInputChanged) {
          if (strlen(messageBuf) > 0) {
            string display_name = (strlen(username) > 0) ? string(username) : "Anonymous";
            string ts = get_current_time_str();
            string new_msg_id = generate_msg_id();
            string safe_name = display_name;
            replace(safe_name.begin(), safe_name.end(), '|', '/');
            string safe_r_name = replyTargetName;
            replace(safe_r_name.begin(), safe_r_name.end(), '|', '/');
            string safe_r_text = replyTargetText;
            replace(safe_r_text.begin(), safe_r_text.end(), '|', '/');
            string packet = "[MSG]|" + new_msg_id + "|" + local_user_id + "|" + safe_name + "|" + local_role + "|" + ts + "|" + safe_r_name + "|" + safe_r_text + "|" + string(messageBuf);
            
            send_raw_line(packet);
            add_chat_log(new_msg_id, display_name, local_user_id, local_role, ts, replyTargetName, replyTargetText, string(messageBuf), true);

            messageBuf[0] = '\0';
            replyTargetId = "";
            replyTargetName = "";
            replyTargetText = "";
            send_typing_status(false);
            ImGui::SetKeyboardFocusHere(-1);
          }
        }
      } else if (currentState == DISCONNECTED_NOTICE) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Chat session ended or peer disconnected.");
        if (ImGui::Button("Return to Main Screen", ImVec2(200, 36))) stop_network();
      }
    }
    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.11f, 0.13f, 0.17f, 1.0f);
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