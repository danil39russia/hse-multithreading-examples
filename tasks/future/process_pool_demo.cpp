#include "process_pool.h"

#include <iostream>
#include <vector>

int main() {
    try {
        hw::ProcessPool pool(4);

        std::vector<hw::MyFuture<int64_t>> futures;
        futures.emplace_back(pool.Submit(hw::TaskKind::kSquare, 9));
        futures.emplace_back(pool.Submit(hw::TaskKind::kTriple, 7));
        futures.emplace_back(pool.Submit(hw::TaskKind::kSafeInverse, 25));
        futures.emplace_back(pool.Submit(hw::TaskKind::kSquare, 12));

        std::cout << "Result 1: " << futures[0].Get() << '\n';
        std::cout << "Result 2: " << futures[1].Get() << '\n';
        std::cout << "Result 3: " << futures[2].Get() << '\n';
        std::cout << "Result 4: " << futures[3].Get() << '\n';

        auto badFuture = pool.Submit(hw::TaskKind::kSafeInverse, 0);
        try {
            (void)badFuture.Get();
        } catch (const std::exception& ex) {
            std::cout << "Expected exception: " << ex.what() << '\n';
        }

        std::cout << "ProcessPool demo finished successfully.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
