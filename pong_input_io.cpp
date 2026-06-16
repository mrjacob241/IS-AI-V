#include "pong_input_io.h"

#include "Vcpu___024root.h"

#include <chrono>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr int64_t INPUT_LATCH_NS = 140000000;

} // namespace

PongInputIO::PongInputIO(bool host_quit_enabled)
    : enabled_(false),
      old_flags_(0),
      old_term_storage_(nullptr),
      stop_(false),
      quit_(false),
      host_quit_enabled_(host_quit_enabled),
      key_state_(0),
      published_char_(0),
      published_seq_(0),
      input_deadline_ns_(0) {
    if (!isatty(STDIN_FILENO))
        return;

    termios* old_term = new termios;
    if (tcgetattr(STDIN_FILENO, old_term) != 0) {
        delete old_term;
        return;
    }

    termios raw = *old_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    old_flags_ = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (old_flags_ < 0) {
        delete old_term;
        return;
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        delete old_term;
        return;
    }
    if (fcntl(STDIN_FILENO, F_SETFL, old_flags_ | O_NONBLOCK) != 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, old_term);
        delete old_term;
        return;
    }

    old_term_storage_ = old_term;
    enabled_ = true;
    reader_ = std::thread(&PongInputIO::reader_loop, this);
}

PongInputIO::~PongInputIO() {
    stop_.store(true, std::memory_order_release);
    if (reader_.joinable())
        reader_.join();

    if (enabled_) {
        termios* old_term = static_cast<termios*>(old_term_storage_);
        tcsetattr(STDIN_FILENO, TCSANOW, old_term);
        fcntl(STDIN_FILENO, F_SETFL, old_flags_);
        delete old_term;
    }
}

bool PongInputIO::enabled() const {
    return enabled_;
}

bool PongInputIO::quit_requested() const {
    return quit_.load(std::memory_order_acquire);
}

uint32_t PongInputIO::key_state() const {
    uint32_t state = key_state_.load(std::memory_order_acquire);
    if (state == 0)
        return 0;

    if (now_ns() > input_deadline_ns_.load(std::memory_order_acquire))
        return 0;
    return state;
}

void PongInputIO::apply_to_cpu(Vcpu& top) const {
    top.rootp->cpu__DOT__io0__DOT__key_state = key_state();

    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (top.rootp->cpu__DOT__io0__DOT__stream_ack == published_seq_ &&
        !pending_chars_.empty()) {
        published_char_ = pending_chars_.front();
        pending_chars_.pop_front();
        published_seq_++;
    }

    top.rootp->cpu__DOT__io0__DOT__stream_char = published_char_;
    top.rootp->cpu__DOT__io0__DOT__stream_seq = published_seq_;
}

void PongInputIO::reader_loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        char ch;
        while (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == '\r')
                ch = '\n';

            {
                std::lock_guard<std::mutex> lock(stream_mutex_);
                pending_chars_.push_back((uint8_t)ch);
            }

            if (ch == 'q' || ch == 'Q') {
                key_state_.store(INPUT_QUIT, std::memory_order_release);
                input_deadline_ns_.store(now_ns() + INPUT_LATCH_NS,
                                         std::memory_order_release);
                if (host_quit_enabled_)
                    quit_.store(true, std::memory_order_release);
            } else if (ch == 'w' || ch == 'W') {
                key_state_.store(INPUT_UP, std::memory_order_release);
                input_deadline_ns_.store(now_ns() + INPUT_LATCH_NS,
                                         std::memory_order_release);
            } else if (ch == 's' || ch == 'S') {
                key_state_.store(INPUT_DOWN, std::memory_order_release);
                input_deadline_ns_.store(now_ns() + INPUT_LATCH_NS,
                                         std::memory_order_release);
            }
        }

        if (now_ns() > input_deadline_ns_.load(std::memory_order_acquire))
            key_state_.store(0, std::memory_order_release);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int64_t PongInputIO::now_ns() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}
