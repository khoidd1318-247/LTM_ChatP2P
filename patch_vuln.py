with open("src/frontend/main.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# Fix 1: Buffer overflow in strcat_s
old_strcat = 'if (ImGui::Button(emojis[i], ImVec2(32, 26)))\n            strcat_s(messageBuf, sizeof(messageBuf), emojis[i]);'
new_strcat = '''if (ImGui::Button(emojis[i], ImVec2(32, 26))) {
            if (strlen(messageBuf) + strlen(emojis[i]) < sizeof(messageBuf)) {
              strcat_s(messageBuf, sizeof(messageBuf), emojis[i]);
            }
          }'''
content = content.replace(old_strcat, new_strcat)

# Fix 2: Handshake protocol corruption
old_handshake = 'send_raw_line("[HANDSHAKE]|" + local_user_id + "|" + display_name + "|" +\n                local_role);'
new_handshake = '''string safe_name = display_name;
  replace(safe_name.begin(), safe_name.end(), '|', '/');
  send_raw_line("[HANDSHAKE]|" + local_user_id + "|" + safe_name + "|" +
                local_role);'''
content = content.replace(old_handshake, new_handshake)

# Fix 3: MSG protocol corruption
old_msg = 'string packet = "[MSG]|" + new_msg_id + "|" + local_user_id + "|" +\n                            display_name + "|" + local_role + "|" + ts + "|" +\n                            replyTargetName + "|" + replyTargetText + "|" +\n                            string(messageBuf);'
new_msg = '''string safe_name = display_name;
            replace(safe_name.begin(), safe_name.end(), '|', '/');
            string safe_r_name = replyTargetName;
            replace(safe_r_name.begin(), safe_r_name.end(), '|', '/');
            string safe_r_text = replyTargetText;
            replace(safe_r_text.begin(), safe_r_text.end(), '|', '/');
            string packet = "[MSG]|" + new_msg_id + "|" + local_user_id + "|" +
                            safe_name + "|" + local_role + "|" + ts + "|" +
                            safe_r_name + "|" + safe_r_text + "|" +
                            string(messageBuf);'''
content = content.replace(old_msg, new_msg)

with open("src/frontend/main.cpp", "w", encoding="utf-8") as f:
    f.write(content)
