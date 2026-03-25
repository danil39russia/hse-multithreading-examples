#include "ipc_mpsc_queue.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    try {
        if (argc < 4) {
            std::cout << "Usage: ./ipc_consumer <shm_path> <bytes> <type>\n";
            std::cout << "Example: ./ipc_consumer /ipc_hw 1048576 1\n";
            return 1;
        }

        const std::string shmPath = argv[1];
        const size_t bytes = static_cast<size_t>(std::stoull(argv[2]));
        const uint32_t wantedType = static_cast<uint32_t>(std::stoul(argv[3]));

        ipc::ConsumerNode consumer(shmPath, bytes);
        std::cout << "[Consumer] started, filtering type=" << wantedType << '\n';

        while (true) {
            std::vector<std::byte> payload;
            if (consumer.RecvType(wantedType, payload)) {
                const std::string text = ipc::BytesToString(payload);
                std::cout << "[Consumer] got: \"" << text << "\"\n";
                if (text == "DONE") {
                    break;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        std::cout << "[Consumer] finished\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "consumer error: " << ex.what() << '\n';
        return 2;
    }
}
