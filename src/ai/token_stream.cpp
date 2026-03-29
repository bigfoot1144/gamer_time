#include "ai/token_stream.h"

#include <utility>

void TokenStream::push(std::string event_text) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push(std::move(event_text));
}

std::vector<std::string> TokenStream::drain() {
    std::vector<std::string> drained;
    std::lock_guard<std::mutex> lock(mutex_);
    while (!events_.empty()) {
        drained.push_back(std::move(events_.front()));
        events_.pop();
    }
    return drained;
}
