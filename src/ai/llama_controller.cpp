#include "ai/llama_controller.h"

#include <utility>

void LlamaController::start(const std::string & model_path) {
    worker_.start(model_path);
    last_status_.clear();
    last_result_.clear();
    push_status_if_changed();
}

void LlamaController::shutdown() {
    worker_.shutdown();
    push_status_if_changed();
}

void LlamaController::submit_prompt(std::string prompt, int max_tokens) {
    LlamaWorker::Job job{};
    job.prompt = std::move(prompt);
    job.max_tokens = max_tokens;
    worker_.submit(job);
    push_status_if_changed();
}

std::vector<AiEvent> LlamaController::drain_events() {
    push_status_if_changed();
    push_result_if_changed();

    std::vector<AiEvent> events;
    for (std::string text : event_stream_.drain()) {
        AiEvent event{};
        event.text = std::move(text);
        if (event.text.rfind("status:", 0) == 0) {
            event.type = AiEventType::Status;
            event.text.erase(0, 7);
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

void LlamaController::push_status_if_changed() {
    const std::string current_status = worker_.last_status();
    if (current_status == last_status_) {
        return;
    }
    last_status_ = current_status;
    event_stream_.push("status:" + current_status);
}

void LlamaController::push_result_if_changed() {
    const std::string current_result = worker_.last_result();
    if (current_result.empty() || current_result == last_result_) {
        return;
    }
    last_result_ = current_result;
    event_stream_.push("completed:" + current_result);
}
