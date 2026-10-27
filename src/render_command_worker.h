#pragma once
#include <array>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

// Experimental backend building block. NOT installed in the D3D9 hooks.
// Backend construction, execution and destruction all happen on the worker.
// Command must own its payload (no borrowed engine memory / COM pointers).
// A ticket means ACCEPTED, never D3D_OK. Native HRESULT-returning calls cannot
// be replaced with Submit unless their result contract is separately proven.
namespace d2vr_render_worker {
template<class Backend, class Command, std::size_t Capacity = 64>
class Queue {
    static_assert(Capacity > 0, "queue must have capacity");
    static_assert(std::is_nothrow_move_constructible<Command>::value,
        "moving a queued packet must not lose accepted work");
public:
    struct Result { bool accepted = false; std::uint64_t ticket = 0; };
    struct Completion { bool complete = false; std::uint64_t firstFailure = 0; };
    Queue() : worker_([this] { Run(); }) {}
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    ~Queue() { Stop(); }

    Result Submit(Command command) {
        std::unique_lock<std::mutex> lock(mutex_);
        space_.wait(lock, [&] { return stopping_ || failed_ || size_ < Capacity; });
        if (stopping_ || failed_) return {};
        const auto ticket = submitted_ + 1;
        slots_[(head_ + size_) % Capacity].emplace(std::move(command));
        submitted_ = ticket;
        ++size_;
        work_.notify_one();
        return {true, ticket};
    }
    Completion Wait(std::uint64_t ticket) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!ticket || ticket > submitted_) return {};
        done_.wait(lock, [&] { return completed_ >= ticket || exited_; });
        return {completed_ >= ticket, failed_ && failed_ <= ticket ? failed_ : 0};
    }
    Completion Drain() {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto target = submitted_;
        done_.wait(lock, [&] { return completed_ >= target || exited_; });
        return {completed_ >= target, failed_};
    }
    // Single controlling producer calls Stop; no calls may outlive this object.
    // Accepted commands are completed before the backend is destroyed. No
    // timeout abandons a live worker and releases its resources underneath it.
    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        work_.notify_all(); space_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
private:
    void Run() noexcept {
        try {
            Backend backend;
            for (;;) {
                std::optional<Command> command;
                std::uint64_t ticket;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    work_.wait(lock, [&] { return stopping_ || size_; });
                    if (!size_) break;
                    command.emplace(std::move(*slots_[head_]));
                    slots_[head_].reset();
                    head_ = (head_ + 1) % Capacity; --size_;
                    ticket = completed_ + 1;
                    space_.notify_one();
                }
                bool ok = false;
                try { ok = backend.Execute(*command); } catch (...) {}
                // Destroy payload before publishing completion: a successful
                // wait is also the lifetime boundary for command-owned data.
                command.reset();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (!ok && !failed_) failed_ = ticket;
                    completed_ = ticket;
                }
                done_.notify_all(); space_.notify_all();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!failed_) failed_ = completed_ + 1;
            stopping_ = true;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            exited_ = true;
        }
        done_.notify_all(); space_.notify_all();
    }
    std::mutex mutex_;
    std::condition_variable work_, space_, done_;
    std::array<std::optional<Command>, Capacity> slots_;
    std::size_t head_ = 0, size_ = 0;
    std::uint64_t submitted_ = 0, completed_ = 0, failed_ = 0;
    bool stopping_ = false, exited_ = false;
    std::thread worker_; // last: all shared state exists before thread starts
};
}
