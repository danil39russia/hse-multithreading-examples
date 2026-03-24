#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ipc {

constexpr uint32_t kMagic = 0x4D505351;  // MPSQ
constexpr uint32_t kProtocolVersion = 1;
constexpr uint32_t kDefaultPayloadSize = 256;

struct MessageHeader {
    uint32_t type{};
    uint32_t length{};
};

struct QueueMeta {
    uint32_t magic{};
    uint32_t protocolVersion{};
    uint32_t slotCount{};
    uint32_t payloadSize{};
    std::atomic<uint64_t> head{0};
    std::atomic<uint64_t> tail{0};
};

struct SlotHeader {
    std::atomic<uint32_t> ready{0};
    MessageHeader message{};
};

class SharedMemoryRegion {
public:
    SharedMemoryRegion(const std::string& name, size_t size, bool createAndInit)
        : name_(name), size_(size) {
        if (name_.empty() || name_[0] != '/') {
            throw std::invalid_argument("shm path must start with '/'");
        }
        if (size_ < sizeof(QueueMeta) + sizeof(SlotHeader) + kDefaultPayloadSize) {
            throw std::invalid_argument("shared memory size is too small");
        }

        fd_ = shm_open(name_.c_str(), O_RDWR | O_CREAT, 0666);
        if (fd_ < 0) {
            throw std::runtime_error("shm_open failed");
        }

        if (createAndInit) {
            if (ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
                close(fd_);
                throw std::runtime_error("ftruncate failed");
            }
        } else {
            struct stat st {};
            if (fstat(fd_, &st) != 0) {
                close(fd_);
                throw std::runtime_error("fstat failed");
            }
            if (static_cast<size_t>(st.st_size) < size_) {
                close(fd_);
                throw std::runtime_error("existing shm is smaller than requested size");
            }
        }

        mapping_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapping_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("mmap failed");
        }

        meta_ = reinterpret_cast<QueueMeta*>(mapping_);
        if (createAndInit) {
            InitializeMeta();
        } else {
            ValidateMeta();
        }
    }

    ~SharedMemoryRegion() {
        if (mapping_ && mapping_ != MAP_FAILED) {
            munmap(mapping_, size_);
        }
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    QueueMeta* Meta() {
        return meta_;
    }

    const QueueMeta* Meta() const {
        return meta_;
    }

    SlotHeader* SlotAt(uint64_t index) {
        const uint64_t slotIndex = index % meta_->slotCount;
        const size_t offset = sizeof(QueueMeta) + slotIndex * SlotStorageSize();
        return reinterpret_cast<SlotHeader*>(reinterpret_cast<std::byte*>(mapping_) + offset);
    }

    std::byte* PayloadAt(uint64_t index) {
        auto* slot = SlotAt(index);
        return reinterpret_cast<std::byte*>(slot) + sizeof(SlotHeader);
    }

    uint32_t PayloadSize() const {
        return meta_->payloadSize;
    }

private:
    size_t SlotStorageSize() const {
        return sizeof(SlotHeader) + meta_->payloadSize;
    }

    void InitializeMeta() {
        std::memset(mapping_, 0, size_);
        meta_->magic = kMagic;
        meta_->protocolVersion = kProtocolVersion;
        meta_->payloadSize = kDefaultPayloadSize;
        meta_->slotCount =
            static_cast<uint32_t>((size_ - sizeof(QueueMeta)) / (sizeof(SlotHeader) + meta_->payloadSize));
        if (meta_->slotCount == 0) {
            throw std::runtime_error("slotCount is zero");
        }
        meta_->head.store(0, std::memory_order_relaxed);
        meta_->tail.store(0, std::memory_order_relaxed);
    }

    void ValidateMeta() const {
        if (meta_->magic != kMagic) {
            throw std::runtime_error("queue magic mismatch");
        }
        if (meta_->protocolVersion != kProtocolVersion) {
            throw std::runtime_error("protocol version mismatch");
        }
        if (meta_->slotCount == 0) {
            throw std::runtime_error("queue is not initialized");
        }
    }

    std::string name_;
    size_t size_{};
    int fd_{-1};
    void* mapping_{nullptr};
    QueueMeta* meta_{nullptr};
};

class ProducerNode {
public:
    ProducerNode(const std::string& path, size_t bytes, bool createAndInit)
        : region_(path, bytes, createAndInit) {
    }

    bool Send(uint32_t type, const void* payload, uint32_t length) {
        if (length > region_.PayloadSize()) {
            return false;
        }

        while (true) {
            uint64_t tail = region_.Meta()->tail.load(std::memory_order_relaxed);
            const uint64_t head = region_.Meta()->head.load(std::memory_order_acquire);
            if (tail - head >= region_.Meta()->slotCount) {
                return false;  // queue is full
            }
            if (region_.Meta()->tail.compare_exchange_weak(
                    tail, tail + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                auto* slot = region_.SlotAt(tail);
                while (slot->ready.load(std::memory_order_acquire) != 0) {
                    // stale slot; another process may still be finishing read
                    std::this_thread::yield();
                }
                slot->message.type = type;
                slot->message.length = length;
                if (length > 0) {
                    std::memcpy(region_.PayloadAt(tail), payload, length);
                }
                slot->ready.store(1, std::memory_order_release);
                return true;
            }
        }
    }

private:
    SharedMemoryRegion region_;
};

class ConsumerNode {
public:
    ConsumerNode(const std::string& path, size_t bytes)
        : region_(path, bytes, false) {
    }

    bool RecvType(uint32_t desiredType, std::vector<std::byte>& outPayload) {
        while (true) {
            MessageHeader header {};
            std::vector<std::byte> payload;
            if (!TryPop(header, payload)) {
                return false;
            }
            if (header.type == desiredType) {
                outPayload = std::move(payload);
                return true;
            }
            // drop message of another type and continue polling
        }
    }

private:
    bool TryPop(MessageHeader& outHeader, std::vector<std::byte>& outPayload) {
        const uint64_t head = region_.Meta()->head.load(std::memory_order_relaxed);
        const uint64_t tail = region_.Meta()->tail.load(std::memory_order_acquire);
        if (head == tail) {
            return false;
        }

        auto* slot = region_.SlotAt(head);
        if (slot->ready.load(std::memory_order_acquire) == 0) {
            return false;
        }

        outHeader = slot->message;
        outPayload.resize(outHeader.length);
        if (outHeader.length > 0) {
            std::memcpy(outPayload.data(), region_.PayloadAt(head), outHeader.length);
        }

        slot->ready.store(0, std::memory_order_release);
        region_.Meta()->head.store(head + 1, std::memory_order_release);
        return true;
    }

    SharedMemoryRegion region_;
};

inline std::string BytesToString(const std::vector<std::byte>& payload) {
    return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
}

}  // namespace ipc
