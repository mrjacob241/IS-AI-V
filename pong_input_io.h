#ifndef PONG_INPUT_IO_H
#define PONG_INPUT_IO_H

#include "Vcpu.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

class PongInputIO {
public:
    enum : uint32_t {
        INPUT_UP = 1u << 0,
        INPUT_DOWN = 1u << 1,
        INPUT_QUIT = 1u << 2
    };

    explicit PongInputIO(bool host_quit_enabled = true);
    ~PongInputIO();

    PongInputIO(const PongInputIO&) = delete;
    PongInputIO& operator=(const PongInputIO&) = delete;

    bool enabled() const;
    bool quit_requested() const;
    uint32_t key_state() const;
    void apply_to_cpu(Vcpu& top) const;

private:
    void reader_loop();
    int64_t now_ns() const;

    bool enabled_;
    int old_flags_;
    void* old_term_storage_;
    std::atomic<bool> stop_;
    std::atomic<bool> quit_;
    bool host_quit_enabled_;
    std::atomic<uint32_t> key_state_;
    mutable std::mutex stream_mutex_;
    mutable std::deque<uint8_t> pending_chars_;
    mutable uint32_t published_char_;
    mutable uint32_t published_seq_;
    std::atomic<int64_t> input_deadline_ns_;
    std::thread reader_;
};

#endif
