#pragma once

#include <mutex>
#include <queue>
#include <string>
#include <vector>

class TokenStream {
public:
    void push(std::string event_text);
    std::vector<std::string> drain();

private:
    std::mutex mutex_;
    std::queue<std::string> events_;
};
