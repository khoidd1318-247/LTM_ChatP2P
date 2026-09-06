#pragma once
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <string>
#include "models.h"

using namespace std;

ImVec4 get_avatar_color(const string &str);
void render_avatar(const string &name, const string &id, float size = 32.0f);
void CustomDebouncedCharCallback(GLFWwindow *window, unsigned int c);
int PortInputFilter(ImGuiInputTextCallbackData *data);
int IPInputFilter(ImGuiInputTextCallbackData *data);
void apply_modern_theme();
void render_chat_bubble(size_t idx, ChatMessage &msg);