#pragma once

#include "ai/token_stream.h"
#include "llama_worker.h"

#include <string>
#include <vector>

enum class AiEventType {
    Status,
    Token,
    Completed,
    Error,
};

struct AiEvent {
    AiEventType type = AiEventType::Status;
    std::string text;
};

class LlamaController {
public:
    void start(const std::string & model_path);
    void shutdown();
    void submit_prompt(std::string prompt, int max_tokens);
    std::vector<AiEvent> drain_events();
    bool is_ready() const;
    bool is_loading() const;

private:
    mutable LlamaWorker worker_;
    TokenStream event_stream_;
};
