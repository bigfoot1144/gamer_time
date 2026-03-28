#pragma once

#include <llama.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class LlamaWorker {
public:
    struct Job {
        std::string prompt;
        int max_tokens = 64;
    };

    LlamaWorker() = default;
    ~LlamaWorker();

    void start(const std::string & model_path);
    void shutdown();

    void submit(const Job & job);

    bool is_ready() const;
    bool is_loading() const;

    std::string last_status() const;
    std::string last_result() const;

private:
    void thread_main(std::string model_path);
    bool load_model(const std::string & model_path);
    std::string run_inference(const Job & job);

    void set_status(std::string status);
    void set_result(std::string result);

private:
    std::thread worker_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Job> jobs_;

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> loading_{false};

    std::string last_status_;
    std::string last_result_;

    llama_model * model_ = nullptr;
    llama_context * ctx_ = nullptr;
    const llama_vocab * vocab_ = nullptr;
};