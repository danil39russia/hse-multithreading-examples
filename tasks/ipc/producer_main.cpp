#include "ipc_mpsc_queue.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    try {
        if (argc < 4) {
            std::cout << "Usage: ./ipc_producer <shm_path> <bytes> <producer_id> [create]\n";
            std::cout << "Example: ./ipc_producer /ipc_hw 1048576 1 create\n";
            return 1;
        }

        const std::string shmPath = argv[1];
        const size_t bytes = static_cast<size_t>(std::stoull(argv[2]));
        const int producerId = std::stoi(argv[3]);
        const bool create = (argc >= 5) && (std::string(argv[4]) == "create");

        ipc::ProducerNode producer(shmPath, bytes, create);
        std::cout << "[Producer " << producerId << "] started\n";

        for (int i = 1; i <= 20; ++i) {
            const uint32_t type = (i % 2 == 0) ? 1U : 2U;
            const std::string text = "producer=" + std::to_string(producerId) + " message=" + std::to_string(i);
            while (!producer.Send(type, text.data(), static_cast<uint32_t>(text.size()))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << "[Producer " << producerId << "] sent type=" << type << " payload=\"" << text << "\"\n";
        }

        if (producerId == 1) {
            const std::string done = "DONE";
            while (!producer.Send(1, done.data(), static_cast<uint32_t>(done.size()))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << "[Producer " << producerId << "] sent type=1 payload=\"DONE\"\n";
        }

        std::cout << "[Producer " << producerId << "] finished\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "producer error: " << ex.what() << '\n';
        return 2;
    }
}
