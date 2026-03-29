#include "ai/llama_controller.h"

#include <utility>

void LlamaController::start(const std::string & model_path) {
    worker_.set_event_callback([this](std::string event_text) {
        event_stream_.push(std::move(event_text));
    });
    worker_.start(model_path);
}

void LlamaController::shutdown() {
    worker_.shutdown();
    worker_.set_event_callback({});
}

void LlamaController::submit_prompt(std::string prompt, int max_tokens) {
    LlamaWorker::Job job{};
    job.prompt = std::move(prompt);
    job.max_tokens = max_tokens;
    worker_.submit(job);
}

std::vector<AiEvent> LlamaController::drain_events() {
    std::vector<AiEvent> events;
    for (std::string text : event_stream_.drain()) {
        AiEvent event{};
        event.text = std::move(text);
        if (event.text.rfind("status:", 0) == 0) {
            event.type = AiEventType::Status;
            event.text.erase(0, 7);
        } else if (event.text.rfind("token:", 0) == 0) {
            event.type = AiEventType::Token;
            event.text.erase(0, 6);
        } else if (event.text.rfind("completed:", 0) == 0) {
            event.type = AiEventType::Completed;
            event.text.erase(0, 10);
        } else if (event.text.rfind("error:", 0) == 0) {
            event.type = AiEventType::Error;
            event.text.erase(0, 6);
        } else {
            event.type = AiEventType::Token;
        }
        events.push_back(std::move(event));
    }
    return events;
}

bool LlamaController::is_ready() const {
    return worker_.is_ready();
}

bool LlamaController::is_loading() const {
    return worker_.is_loading();
}
