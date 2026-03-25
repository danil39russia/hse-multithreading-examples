#pragma once

#include <optional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <stdexcept>

template <class T>
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : capacity_(ValidateSize(size)) {
    }

    void Send(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_send_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            throw std::runtime_error("BufferedChannel is closed");
        }
        queue_.push_back(value);
        cv_recv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_recv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop_front();
        cv_send_.notify_one();
        return value;
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_send_.notify_all();
        cv_recv_.notify_all();
    }

private:
    static size_t ValidateSize(int size) {
        if (size < 1) {
            throw std::invalid_argument("BufferedChannel: capacity must be >= 1");
        }
        return static_cast<size_t>(size);
    }

    std::mutex mutex_;
    std::condition_variable cv_send_;
    std::condition_variable cv_recv_;
    std::deque<T> queue_;
    size_t capacity_;
    bool closed_{false};
};
