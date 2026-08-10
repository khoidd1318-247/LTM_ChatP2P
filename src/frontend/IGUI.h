#pragma once
#include <string>

// Abstract UI Interface (Frontend Interface)
class IGUI {
public:
    virtual ~IGUI() {}
    virtual void addOrUpdateMessage(const std::string& msgId, const std::string& sender, const std::string& msg, bool isSystem = false) = 0;
    virtual void revokeMessage(const std::string& msgId) = 0;
    virtual void updateStatus(const std::string& status) = 0;
    virtual void setTheme(bool dark) = 0;
    virtual void printSystemMessage(const std::string& msg) = 0;
    virtual void updateFileProgress(const std::string& filename, size_t processed, size_t total) = 0;
};
