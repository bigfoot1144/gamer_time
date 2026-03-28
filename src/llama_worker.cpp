#include "llama_worker.h"

#include <sstream>
#include <utility>
#include <vector>
#include <iostream>

namespace {

std::string token_to_string(const llama_vocab * vocab, llama_token token) {
    char piece[512];
    const int n = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, true);
    if (n < 0) {
        return {};
    }
    return std::string(piece, piece + n);
}

} // namespace

LlamaWorker::~LlamaWorker() {
    shutdown();
}

void LlamaWorker::start(const std::string & model_path) {
    shutdown();

    stop_requested_ = false;
    ready_ = false;
    loading_ = false;
    set_status("Starting");

    worker_thread_ = std::thread(&LlamaWorker::thread_main, this, model_path);
}

void LlamaWorker::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        while (!jobs_.empty()) {
            jobs_.pop();
        }
    }
    cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }

    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }

    vocab_ = nullptr;
    ready_ = false;
    loading_ = false;
}

void LlamaWorker::submit(const Job & job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(job);
    }
    cv_.notify_one();
}

bool LlamaWorker::is_ready() const {
    return ready_.load();
}

bool LlamaWorker::is_loading() const {
    return loading_.load();
}

std::string LlamaWorker::last_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_status_;
}

std::string LlamaWorker::last_result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_result_;
}

void LlamaWorker::set_status(std::string status) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_status_ = std::move(status);
}

void LlamaWorker::set_result(std::string result) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_result_ = std::move(result);
}

bool LlamaWorker::load_model(const std::string & model_path) {
    loading_ = true;
    ready_ = false;
    set_status("Initializing llama backend...");

    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_ = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model_) {
        set_status("Failed to load model");
        loading_ = false;
        return false;
    }

    vocab_ = llama_model_get_vocab(model_);
    if (!vocab_) {
        set_status("Failed to get vocab");
        loading_ = false;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;

    // ⚠️ If this fails to compile, switch to:
    // ctx_ = llama_new_context_with_model(model_, ctx_params);
    ctx_ = llama_init_from_model(model_, ctx_params);

    if (!ctx_) {
        set_status("Failed to create context");
        loading_ = false;
        return false;
    }

    ready_ = true;
    loading_ = false;
    set_status("Ready");
    return true;
}

void LlamaWorker::thread_main(std::string model_path) {
    if (model_path.empty()) {
        set_status("No model path provided");
        return;
    }

    if (!load_model(model_path)) {
        return;
    }

    while (true) {
        Job job;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] {
                return stop_requested_ || !jobs_.empty();
            });

            if (stop_requested_) {
                break;
            }

            job = jobs_.front();
            jobs_.pop();
        }

        set_status("Running inference...");
        const std::string output = run_inference(job);
        set_result(output);
        std::cout << "\n=== Llama Output ===\n" << output << "\n";
        set_status("Ready");
    }

    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }

    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }

    vocab_ = nullptr;
    ready_ = false;
    loading_ = false;

    llama_backend_free();
}

std::string LlamaWorker::run_inference(const Job & job) {
    if (!ctx_ || !model_ || !vocab_) {
        return "llama worker not initialized";
    }

    const std::string prompt = job.prompt.empty() ? std::string("Hello") : job.prompt;
    const int max_tokens = job.max_tokens > 0 ? job.max_tokens : 64;

    std::vector<llama_token> prompt_tokens(prompt.size() + 8);

    int num_prompt_tokens = llama_tokenize(
        vocab_,
        prompt.c_str(),
        static_cast<int32_t>(prompt.size()),
        prompt_tokens.data(),
        static_cast<int32_t>(prompt_tokens.size()),
        true,
        true
    );

    if (num_prompt_tokens < 0) {
        prompt_tokens.resize(static_cast<size_t>(-num_prompt_tokens));
        num_prompt_tokens = llama_tokenize(
            vocab_,
            prompt.c_str(),
            static_cast<int32_t>(prompt.size()),
            prompt_tokens.data(),
            static_cast<int32_t>(prompt_tokens.size()),
            true,
            true
        );
    }

    if (num_prompt_tokens <= 0) {
        return "tokenization failed";
    }

    prompt_tokens.resize(static_cast<size_t>(num_prompt_tokens));

    llama_batch batch = llama_batch_get_one(
        prompt_tokens.data(),
        static_cast<int32_t>(prompt_tokens.size())
    );

    if (llama_decode(ctx_, batch) != 0) {
        return "initial decode failed";
    }

    std::ostringstream out;
    out << prompt;

    for (int i = 0; i < max_tokens; ++i) {
        const float * logits = llama_get_logits(ctx_);
        if (!logits) {
            break;
        }

        const int32_t n_vocab = llama_vocab_n_tokens(vocab_);
        llama_token best_token = 0;
        float best_logit = logits[0];

        for (int32_t tok = 1; tok < n_vocab; ++tok) {
            if (logits[tok] > best_logit) {
                best_logit = logits[tok];
                best_token = tok;
            }
        }

        if (llama_vocab_is_eog(vocab_, best_token)) {
            break;
        }

        out << token_to_string(vocab_, best_token);

        llama_token next_token = best_token;
        llama_batch next_batch = llama_batch_get_one(&next_token, 1);

        if (llama_decode(ctx_, next_batch) != 0) {
            break;
        }
    }

    return out.str();
}