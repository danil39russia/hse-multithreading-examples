#pragma once

#include <atomic>

#ifdef __linux__

#include <cerrno>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

class FutexMutex {
public:
    FutexMutex() = default;
    FutexMutex(const FutexMutex&) = delete;
    FutexMutex& operator=(const FutexMutex&) = delete;

    void lock() {
        int current = 0;
        if (state_.compare_exchange_strong(current, 1, std::memory_order_acquire)) {
            return;
        }

        do {
            if (current == 2 || state_.exchange(2, std::memory_order_acquire) != 0) {
                FutexWait(2);
            }
            current = 0;
        } while (!state_.compare_exchange_strong(current, 2, std::memory_order_acquire));
    }

    bool try_lock() {
        int expected = 0;
        return state_.compare_exchange_strong(expected, 1, std::memory_order_acquire);
    }

    void unlock() {
        if (state_.fetch_sub(1, std::memory_order_release) != 1) {
            state_.store(0, std::memory_order_release);
            FutexWake(1);
        }
    }

private:
    void FutexWait(int expected) {
        while (syscall(SYS_futex,
                       reinterpret_cast<int*>(&state_),
                       FUTEX_WAIT_PRIVATE,
                       expected,
                       nullptr,
                       nullptr,
                       0) == -1) {
            if (errno != EINTR) {
                break;
            }
        }
    }

    void FutexWake(int count) {
        syscall(SYS_futex,
                reinterpret_cast<int*>(&state_),
                FUTEX_WAKE_PRIVATE,
                count,
                nullptr,
                nullptr,
                0);
    }

    std::atomic<int> state_{0};
};

#else

#include <mutex>

class FutexMutex {
public:
    FutexMutex() = default;
    FutexMutex(const FutexMutex&) = delete;
    FutexMutex& operator=(const FutexMutex&) = delete;

    void lock() {
        fallback_.lock();
    }

    bool try_lock() {
        return fallback_.try_lock();
    }

    void unlock() {
        fallback_.unlock();
    }

private:
    std::mutex fallback_;
};

#endif
