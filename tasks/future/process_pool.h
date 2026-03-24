#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace hw {

enum class TaskKind : uint32_t {
    kSquare = 1,
    kTriple = 2,
    kSafeInverse = 3
};

template <class T>
class SharedState {
public:
    void SetValue(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) {
                return;
            }
            value_ = std::move(value);
            ready_ = true;
        }
        cv_.notify_all();
    }

    void SetException(std::exception_ptr ex) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ready_) {
                return;
            }
            exception_ = ex;
            ready_ = true;
        }
        cv_.notify_all();
    }

    T Get() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return ready_; });
        if (exception_) {
            std::rethrow_exception(exception_);
        }
        return *value_;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_{false};
    std::optional<T> value_;
    std::exception_ptr exception_;
};

template <class T>
class MyFuture {
public:
    MyFuture() = default;
    explicit MyFuture(std::shared_ptr<SharedState<T>> state) : state_(std::move(state)) {}

    MyFuture(MyFuture&&) noexcept = default;
    MyFuture& operator=(MyFuture&&) noexcept = default;
    MyFuture(const MyFuture&) = delete;
    MyFuture& operator=(const MyFuture&) = delete;

    T Get() {
        if (!state_) {
            throw std::runtime_error("future has no state");
        }
        return state_->Get();
    }

private:
    std::shared_ptr<SharedState<T>> state_;
};

struct TaskMessage {
    uint64_t id{};
    uint32_t kind{};
    int64_t input{};
    uint32_t shutdown{};
};

struct ResultMessage {
    uint64_t id{};
    int64_t value{};
    uint32_t hasError{};
    std::array<char, 64> error{};
};

inline void WriteExact(int fd, const void* data, size_t bytes) {
    const auto* ptr = static_cast<const uint8_t*>(data);
    size_t written = 0;
    while (written < bytes) {
        const ssize_t n = write(fd, ptr + written, bytes - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("write failed");
        }
        written += static_cast<size_t>(n);
    }
}

inline bool ReadExact(int fd, void* data, size_t bytes) {
    auto* ptr = static_cast<uint8_t*>(data);
    size_t received = 0;
    while (received < bytes) {
        const ssize_t n = read(fd, ptr + received, bytes - received);
        if (n == 0) {
            return false;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("read failed");
        }
        received += static_cast<size_t>(n);
    }
    return true;
}

class ProcessPool {
public:
    explicit ProcessPool(size_t workerCount)
        : workerCount_(workerCount == 0 ? 1 : workerCount) {
        workers_.reserve(workerCount_);
        for (size_t i = 0; i < workerCount_; ++i) {
            workers_.push_back(SpawnWorker());
        }
        collectorThread_ = std::thread([this] { CollectLoop(); });
    }

    ~ProcessPool() {
        Shutdown();
    }

    ProcessPool(const ProcessPool&) = delete;
    ProcessPool& operator=(const ProcessPool&) = delete;

    MyFuture<int64_t> Submit(TaskKind kind, int64_t input) {
        std::lock_guard<std::mutex> lock(submitMutex_);
        if (stopped_) {
            throw std::runtime_error("ProcessPool is stopped");
        }

        const uint64_t id = nextId_.fetch_add(1, std::memory_order_relaxed);
        auto state = std::make_shared<SharedState<int64_t>>();
        {
            std::lock_guard<std::mutex> lockMap(statesMutex_);
            states_.emplace(id, state);
        }

        TaskMessage msg {};
        msg.id = id;
        msg.kind = static_cast<uint32_t>(kind);
        msg.input = input;
        msg.shutdown = 0;

        Worker& worker = workers_[nextWorker_++ % workers_.size()];
        WriteExact(worker.taskWriteFd, &msg, sizeof(msg));
        return MyFuture<int64_t>(state);
    }

    void Shutdown() {
        std::lock_guard<std::mutex> lock(submitMutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;

        for (auto& worker : workers_) {
            TaskMessage msg {};
            msg.shutdown = 1;
            try {
                WriteExact(worker.taskWriteFd, &msg, sizeof(msg));
            } catch (...) {
                // worker may already be down
            }
            close(worker.taskWriteFd);
            worker.taskWriteFd = -1;
        }

        for (auto& worker : workers_) {
            waitpid(worker.pid, nullptr, 0);
            if (worker.resultReadFd >= 0) {
                close(worker.resultReadFd);
                worker.resultReadFd = -1;
            }
        }

        if (collectorThread_.joinable()) {
            collectorThread_.join();
        }

        std::lock_guard<std::mutex> lockMap(statesMutex_);
        for (auto& [id, state] : states_) {
            (void)id;
            state->SetException(std::make_exception_ptr(std::runtime_error("pool stopped")));
        }
        states_.clear();
    }

private:
    struct Worker {
        pid_t pid{-1};
        int taskWriteFd{-1};
        int resultReadFd{-1};
    };

    static int64_t Execute(TaskKind kind, int64_t input) {
        switch (kind) {
            case TaskKind::kSquare:
                return input * input;
            case TaskKind::kTriple:
                return input * 3;
            case TaskKind::kSafeInverse:
                if (input == 0) {
                    throw std::runtime_error("division by zero");
                }
                return 1000 / input;
            default:
                throw std::runtime_error("unknown task kind");
        }
    }

    static void WorkerLoop(int taskReadFd, int resultWriteFd) {
        try {
            while (true) {
                TaskMessage task {};
                if (!ReadExact(taskReadFd, &task, sizeof(task))) {
                    break;
                }
                if (task.shutdown) {
                    break;
                }

                ResultMessage result {};
                result.id = task.id;
                try {
                    result.value = Execute(static_cast<TaskKind>(task.kind), task.input);
                    result.hasError = 0;
                } catch (const std::exception& ex) {
                    result.hasError = 1;
                    std::strncpy(result.error.data(), ex.what(), result.error.size() - 1);
                    result.error.back() = '\0';
                }
                WriteExact(resultWriteFd, &result, sizeof(result));
            }
        } catch (...) {
            // If a worker fails, it just exits; parent side will convert pending futures to exception on shutdown.
        }
        close(taskReadFd);
        close(resultWriteFd);
        _exit(0);
    }

    Worker SpawnWorker() {
        int taskPipe[2] {};
        int resultPipe[2] {};
        if (pipe(taskPipe) != 0 || pipe(resultPipe) != 0) {
            throw std::runtime_error("pipe failed");
        }

        const pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error("fork failed");
        }

        if (pid == 0) {
            close(taskPipe[1]);
            close(resultPipe[0]);
            WorkerLoop(taskPipe[0], resultPipe[1]);
        }

        close(taskPipe[0]);
        close(resultPipe[1]);
        return Worker {.pid = pid, .taskWriteFd = taskPipe[1], .resultReadFd = resultPipe[0]};
    }

    void CollectLoop() {
        while (true) {
            bool gotAny = false;
            for (auto& worker : workers_) {
                if (worker.resultReadFd < 0) {
                    continue;
                }

                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(worker.resultReadFd, &readSet);
                timeval timeout {.tv_sec = 0, .tv_usec = 1000};
                const int ready = select(worker.resultReadFd + 1, &readSet, nullptr, nullptr, &timeout);
                if (ready <= 0 || !FD_ISSET(worker.resultReadFd, &readSet)) {
                    continue;
                }

                ResultMessage result {};
                if (!ReadExact(worker.resultReadFd, &result, sizeof(result))) {
                    close(worker.resultReadFd);
                    worker.resultReadFd = -1;
                    continue;
                }
                gotAny = true;
                Fulfill(result);
            }

            if (stopped_) {
                return;
            }

            if (!gotAny) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void Fulfill(const ResultMessage& result) {
        std::shared_ptr<SharedState<int64_t>> state;
        {
            std::lock_guard<std::mutex> lock(statesMutex_);
            auto it = states_.find(result.id);
            if (it == states_.end()) {
                return;
            }
            state = it->second;
            states_.erase(it);
        }

        if (result.hasError) {
            state->SetException(std::make_exception_ptr(std::runtime_error(result.error.data())));
        } else {
            state->SetValue(result.value);
        }
    }

    size_t workerCount_{};
    std::vector<Worker> workers_;
    std::thread collectorThread_;

    std::atomic<uint64_t> nextId_{1};
    size_t nextWorker_{0};

    std::mutex submitMutex_;
    bool stopped_{false};

    std::mutex statesMutex_;
    std::unordered_map<uint64_t, std::shared_ptr<SharedState<int64_t>>> states_;
};

}  // namespace hw
