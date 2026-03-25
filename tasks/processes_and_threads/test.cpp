#include "apply_function.h"

#include <gtest/gtest.h>

#include <vector>
#include <numeric>
#include <string>
#include <cmath>

TEST(ApplyFunction, SingleThreadDoubleValues) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    ApplyFunction<int>(data, [](int& x) { x *= 2; }, 1);
    EXPECT_EQ(data, (std::vector<int>{2, 4, 6, 8, 10}));
}

TEST(ApplyFunction, MultipleThreadsDoubleValues) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    ApplyFunction<int>(data, [](int& x) { x *= 2; }, 4);
    EXPECT_EQ(data, (std::vector<int>{2, 4, 6, 8, 10, 12, 14, 16}));
}

TEST(ApplyFunction, MoreThreadsThanElements) {
    std::vector<int> data = {10, 20, 30};
    ApplyFunction<int>(data, [](int& x) { x += 1; }, 100);
    EXPECT_EQ(data, (std::vector<int>{11, 21, 31}));
}

TEST(ApplyFunction, EmptyVector) {
    std::vector<int> data;
    ApplyFunction<int>(data, [](int& x) { x *= 2; }, 4);
    EXPECT_TRUE(data.empty());
}

TEST(ApplyFunction, SingleElement) {
    std::vector<int> data = {42};
    ApplyFunction<int>(data, [](int& x) { x = x * x; }, 3);
    EXPECT_EQ(data, (std::vector<int>{1764}));
}

TEST(ApplyFunction, StringType) {
    std::vector<std::string> data = {"hello", "world", "foo"};
    ApplyFunction<std::string>(data, [](std::string& s) { s += "!"; }, 2);
    EXPECT_EQ(data, (std::vector<std::string>{"hello!", "world!", "foo!"}));
}

TEST(ApplyFunction, LargeVectorCorrectness) {
    const int n = 100000;
    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 0);

    std::vector<int> expected(n);
    std::iota(expected.begin(), expected.end(), 0);
    for (auto& x : expected) {
        x = x * x;
    }

    ApplyFunction<int>(data, [](int& x) { x = x * x; }, 8);
    EXPECT_EQ(data, expected);
}

TEST(ApplyFunction, DefaultThreadCount) {
    std::vector<int> data = {1, 2, 3};
    ApplyFunction<int>(data, [](int& x) { x += 10; });
    EXPECT_EQ(data, (std::vector<int>{11, 12, 13}));
}

TEST(ApplyFunction, DoubleType) {
    std::vector<double> data = {1.0, 4.0, 9.0, 16.0};
    ApplyFunction<double>(data, [](double& x) { x = std::sqrt(x); }, 2);
    for (int i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(data[i], static_cast<double>(i + 1));
    }
}

TEST(ApplyFunction, UnevenSplit) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
    ApplyFunction<int>(data, [](int& x) { x *= 3; }, 4);
    EXPECT_EQ(data, (std::vector<int>{3, 6, 9, 12, 15, 18, 21}));
}
