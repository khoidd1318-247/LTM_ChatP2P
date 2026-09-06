#include "ui_helpers.h"
#include "globals.h"
#include "network.h"
#include "imgui_impl_glfw.h"
#include <cctype>
#include <chrono>
#include <algorithm>
#include <map>

using namespace std;

static unsigned int g_LastChar = 0;
static chrono::steady_clock::time_point g_LastCharTime;

ImVec4 get_avatar_color(const string &str) {
  unsigned int hash = 5381;
  for (char c : str) {
    hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
  }
  float r = 0.35f + ((hash & 0xFF) % 150) / 300.0f;
  float g = 0.35f + (((hash >> 8) & 0xFF) % 150) / 300.0f;
  float b = 0.35f + (((hash >> 16) & 0xFF) % 150) / 300.0f;
  return ImVec4(r, g, b, 1.0f);
}

void render_avatar(const string &name, const string &id, float size) {
  string initial = "?";
  if (!name.empty() && name != "Anonymous") {
    initial = (char)toupper(name[0]);
  } else if (!id.empty()) {
    initial = (char)toupper(id[1]);
  }
  ImVec4 col = get_avatar_color(name + id);
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  ImVec2 center = ImVec2(p.x + size * 0.5f, p.y + size * 0.5f);
  draw_list->AddCircleFilled(center, size * 0.5f, ImGui::ColorConvertFloat4ToU32(col));

  ImVec2 text_sz = ImGui::CalcTextSize(initial.c_str());
  ImVec2 text_pos = ImVec2(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.5f);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), initial.c_str());
  ImGui::Dummy(ImVec2(size, size));
}

void CustomDebouncedCharCallback(GLFWwindow *window, unsigned int c) {
  auto now = chrono::steady_clock::now();
  auto elapsedMs = chrono::duration_cast<chrono::milliseconds>(now - g_LastCharTime).count();
  if (c == g_LastChar && elapsedMs < 35) return;
  g_LastChar = c;
  g_LastCharTime = now;
  ImGui_ImplGlfw_CharCallback(window, c);
}

int PortInputFilter(ImGuiInputTextCallbackData *data) {
  return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
}

int IPInputFilter(ImGuiInputTextCallbackData *data) {
  return ((data->EventChar >= '0' && data->EventChar <= '9') ||
          (data->EventChar >= 'a' && data->EventChar <= 'z') ||
          (data->EventChar >= 'A' && data->EventChar <= 'Z') ||
          data->EventChar == '.') ? 0 : 1;
}

void apply_modern_theme() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.ChildRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.WindowPadding = ImVec2(14, 14);
  style.FramePadding = ImVec2(10, 8);
  style.ItemSpacing = ImVec2(10, 10);
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.17f, 0.22f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.22f, 0.26f, 0.32f, 0.50f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.21f, 0.27f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.33f, 0.42f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.52f, 0.98f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.32f, 0.44f, 0.92f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.52f, 0.98f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.36f, 0.82f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.22f, 0.26f, 0.33f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.33f, 0.42f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.40f, 0.50f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.26f, 0.32f, 0.80f);
}

void render_chat_bubble(size_t idx, ChatMessage &msg) {
  ImGui::Spacing();
  string editedMark = (msg.is_edited && !msg.is_deleted) ? " (edited)" : "";
  string headerText =
      msg.is_self
          ? ("You " + msg.sender_id + " • " + msg.timestamp + editedMark)
          : (msg.sender_name + " " + msg.sender_id + " • " + msg.role + " • " +
             msg.timestamp + editedMark);

  string statusLabel = "";
  ImVec4 statusColor = ImVec4(0.65f, 0.68f, 0.72f, 1.00f);
  if (msg.is_self) {
    if (msg.is_read) {
      statusLabel = "[Đã xem]";
      statusColor = ImVec4(0.40f, 0.80f, 0.45f, 1.00f);
    } else if (msg.is_delivered) {
      statusLabel = "[Đã nhận]";
      statusColor = ImVec4(0.40f, 0.65f, 1.00f, 1.00f);
    } else {
      statusLabel = "[Đã gửi]";
      statusColor = ImVec4(0.65f, 0.68f, 0.72f, 1.00f);
    }
  }

  string displayContent =
      msg.is_deleted ? "This message was deleted." : msg.content;
  bool showReplyPreview = !msg.is_deleted && !msg.reply_to_text.empty();

  float maxBubbleWidth = max(340.0f, min(540.0f, ImGui::GetWindowWidth() * 0.75f));
  ImVec2 textSize = ImGui::CalcTextSize(displayContent.c_str(), NULL, false, maxBubbleWidth - 24.0f);
  float headerWidth = ImGui::CalcTextSize(headerText.c_str()).x;
  if (!statusLabel.empty())
    headerWidth += ImGui::CalcTextSize(statusLabel.c_str()).x + 6.0f;
  float replyWidth = !showReplyPreview ? 0.0f : ImGui::CalcTextSize(("Replying to " + msg.reply_to_name + ": " + msg.reply_to_text).c_str(), NULL, false, maxBubbleWidth - 24.0f).x;
  float bubbleWidth = max(340.0f, min(maxBubbleWidth, max({headerWidth, textSize.x, replyWidth}) + 30.0f));

  if (msg.is_self) {
    float posX = max(10.0f, ImGui::GetWindowWidth() - bubbleWidth - 25.0f);
    ImGui::SetCursorPosX(posX);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.38f, 0.85f, 0.95f)); 
  } else {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.21f, 0.27f, 0.95f));
  }

  ImGui::BeginChild(msg.id.c_str(), ImVec2(bubbleWidth, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
  {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
      ImVec2 p_min = ImGui::GetWindowPos();
      ImVec2 p_max = ImVec2(p_min.x + ImGui::GetWindowSize().x, p_min.y + ImGui::GetWindowSize().y);
      ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 20), 8.0f);
    }

    ImGui::TextColored(msg.is_self ? ImVec4(0.85f, 0.90f, 1.00f, 1.00f) : ImVec4(0.45f, 0.75f, 1.00f, 1.00f), "%s", headerText.c_str());
    if (!statusLabel.empty()) {
      ImGui::SameLine(0.0f, 6.0f);
      ImGui::TextColored(statusColor, "%s", statusLabel.c_str());
    }

    if (msg.is_deleted) {
      ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.00f), "This message was deleted.");
    } else if (editingMessageId == msg.id) {
      if (showReplyPreview) {
        ImGui::TextColored(ImVec4(0.70f, 0.80f, 1.00f, 0.90f), "Replying to %s: \"%s\"", msg.reply_to_name.c_str(), msg.reply_to_text.c_str());
        ImGui::Separator();
      }

      ImGui::PushItemWidth(bubbleWidth - 24.0f);
      bool editEnterPressed = ImGui::InputText(("##edit_" + msg.id).c_str(), editMessageBuf, IM_ARRAYSIZE(editMessageBuf), ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::PopItemWidth();

      ImGui::PushID((int)idx);
      if (ImGui::SmallButton("Save") || editEnterPressed) {
        if (strlen(editMessageBuf) > 0) {
          msg.content = string(editMessageBuf);
          msg.is_edited = true;
          send_raw_line("[EDIT]|" + msg.id + "|" + msg.content);
        }
        editingMessageId = "";
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Cancel")) {
        editingMessageId = "";
      }
      ImGui::PopID();
    } else {
      if (showReplyPreview) {
        ImGui::TextColored(ImVec4(0.70f, 0.80f, 1.00f, 0.90f), "Replying to %s: \"%s\"", msg.reply_to_name.c_str(), msg.reply_to_text.c_str());
        ImGui::Separator();
      }

      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
      
      ImGui::InputTextMultiline(("##msg_" + msg.id).c_str(), (char*)displayContent.c_str(), displayContent.size() + 1, ImVec2(maxBubbleWidth - 24.0f, textSize.y + ImGui::GetStyle().FramePadding.y * 2), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll | 1 << 24); 
      
      ImGui::PopStyleColor(3);

      map<string, pair<int, bool>> reactionCounts;
      for (const auto &rItem : msg.reactions) {
        auto &entry = reactionCounts[rItem.emoji];
        entry.first++;
        if (rItem.user_id == local_user_id) entry.second = true;
      }

      if (!reactionCounts.empty()) {
        ImGui::Spacing();
        ImGui::PushID(("reactions_" + msg.id).c_str());
        int rIdx = 0;
        for (auto &kv : reactionCounts) {
          string emoji = kv.first;
          int count = kv.second.first;
          bool myReaction = kv.second.second;
          if (rIdx > 0) ImGui::SameLine();

          string badgeText = emoji + " " + to_string(count) + (myReaction ? " ✓" : "");

          if (myReaction) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.95f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 1.00f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
          } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.30f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.32f, 0.42f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.90f, 0.95f, 1.00f));
          }

          if (ImGui::SmallButton(badgeText.c_str())) {
            send_raw_line("[REACTION]|" + msg.id + "|" + emoji + "|" + local_user_id);
            auto it = find_if(msg.reactions.begin(), msg.reactions.end(),
                              [&](const ReactionItem &item) { return item.emoji == emoji && item.user_id == local_user_id; });
            if (it != msg.reactions.end()) msg.reactions.erase(it);
            else msg.reactions.push_back({emoji, local_user_id});
          }
          ImGui::PopStyleColor(3);
          rIdx++;
        }
        ImGui::PopID();
      }

      ImGui::Spacing();
      ImGui::PushID((int)idx);
      if (msg.is_self) {
        if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(msg.content.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Edit")) {
          editingMessageId = msg.id;
          strcpy_s(editMessageBuf, sizeof(editMessageBuf), msg.content.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
          msg.is_deleted = true;
          send_raw_line("[DELETE]|" + msg.id);
        }
        ImGui::SameLine();
      } else {
        if (ImGui::SmallButton("Reply")) {
          replyTargetId = msg.id;
          replyTargetName = msg.sender_name;
          replyTargetText = msg.content.substr(0, 35);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Forward")) {
          string forwardContent = "[Forwarded from " + msg.sender_name + "]: " + msg.content;
          snprintf(messageBuf, sizeof(messageBuf), "%s", forwardContent.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(msg.content.c_str());
        ImGui::SameLine();
      }

      const char *quickIcons[] = {"❤️", "⭐", "✨", "✔", "✖"};
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
      for (int k = 0; k < 5; k++) {
        if (k > 0) ImGui::SameLine();
        string emoji = quickIcons[k];
        bool isMyReacted = any_of(msg.reactions.begin(), msg.reactions.end(), [&](const ReactionItem &r) { return r.emoji == emoji && r.user_id == local_user_id; });

        if (isMyReacted) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 1.00f, 1.00f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 1.00f, 1.00f));
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.26f, 0.40f, 0.60f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.40f, 0.60f, 1.00f));
        }

        if (ImGui::Button(quickIcons[k], ImVec2(32, 26))) {
          send_raw_line("[REACTION]|" + msg.id + "|" + emoji + "|" + local_user_id);
          auto it = find_if(msg.reactions.begin(), msg.reactions.end(), [&](const ReactionItem &item) { return item.emoji == emoji && item.user_id == local_user_id; });
          if (it != msg.reactions.end()) msg.reactions.erase(it);
          else msg.reactions.push_back({emoji, local_user_id});
        }
        ImGui::PopStyleColor(2);
      }
      ImGui::PopStyleVar();
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}