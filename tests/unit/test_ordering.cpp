#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <vector>
#include <algorithm>
#include <random>

TEST_SUITE("VebTree Ordering and Iteration") {
    TEST_CASE("toarray returns sorted order") {
        VebTree tree;
        tree.insert(100);
        tree.insert(1);
        tree.insert(50);
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 50);
        REQUIRE(arr[2] == 100);
    }

    TEST_CASE("toarray maintains order after removals") {
        VebTree tree;
        tree.insert(100);
        tree.insert(1);
        tree.insert(50);
        
        tree.remove(50);
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == 2);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 100);
    }

    TEST_CASE("iteration order matches toarray") {
        VebTree tree;
        std::vector<size_t> vals{42, 17, 93, 5, 88, 31};
        for (auto v : vals) {
            tree.insert(v);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        std::vector<size_t> via_iter;
        for (auto v : tree) {
            via_iter.push_back(v);
        }
        
        REQUIRE(arr == via_iter);
        std::sort(vals.begin(), vals.end());
        REQUIRE(arr == vals);
    }

    TEST_CASE("empty tree iteration") {
        VebTree tree;
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.empty());
        
        int count = 0;
        for (auto v : tree) {
            (void)v;
            count++;
        }
        REQUIRE(count == 0);
    }

    TEST_CASE("single element iteration") {
        VebTree tree;
        tree.insert(42);
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == 1);
        REQUIRE(arr[0] == 42);
    }

    TEST_CASE("dense range ordering") {
        VebTree tree;
        for (size_t i = 0; i < 1000; ++i) {
            tree.insert(i);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == 1000);
        
        for (size_t i = 0; i < 1000; ++i) {
            REQUIRE(arr[i] == i);
        }
    }

    TEST_CASE("sparse range ordering") {
        VebTree tree;
        std::vector<size_t> vals;
        for (size_t i = 0; i < 100; ++i) {
            vals.push_back(i * 100);
            tree.insert(i * 100);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr == vals);
    }

    TEST_CASE("large array ordering") {
        VebTree tree;
        std::vector<size_t> vals{
            1000, 10, 500, 250, 999, 0, 750, 333, 666, 123, 456, 789, 234, 345, 890,
            432, 321, 111, 222, 444, 555, 534, 777, 888, 9999, 8888, 7777, 6666,
            5555, 4444, 3333, 8901, 9012, 10000, 15000, 20000, 25000, 30000, 35000,
            23234, 24234, 25234, 26234, 27234, 28234, 29234, 30234, 31234, 32234,
            33234, 34234, 35234, 36234, 37234, 38234, 39234, 7135, 8246, 10234,
            11234, 12234, 13234, 14234, 15234, 16234, 17234, 18234, 19234, 20234,
            5284, 21234, 22234, 2222, 1111, 1234, 2345, 3456, 4567, 5678, 6789,
            7890, 1357, 2468, 3690, 1470, 2580, 3691, 6846, 4802, 5913, 6024,
            9357, 10468, 11579, 12680, 13791, 14802, 15913, 16024, 17135, 18246,
            19357, 20468, 21579, 22680, 23791, 24802, 25913, 27024, 28135, 29246,
            30357, 31468, 32579, 33680, 34791, 35802, 36913, 37024, 26489, 254035,
            123456, 234567, 345678, 456789, 567890, 678901, 789012, 890123, 901234,
            634610, 745721, 856832, 967943
        };
        
        for (auto v : vals) {
            tree.insert(v);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        std::sort(vals.begin(), vals.end());
        
        REQUIRE(arr == vals);
    }

    TEST_CASE("ordering with node type transitions") {
        VebTree tree;
        std::vector<size_t> vals;
        
        for (size_t i = 0; i < 100; ++i) {
            vals.push_back(i);
            tree.insert(i);
        }
        
        for (size_t i = 256; i < 512; ++i) {
            vals.push_back(i);
            tree.insert(i);
        }
        
        for (size_t i = 70000; i < 70100; ++i) {
            vals.push_back(i);
            tree.insert(i);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == vals.size());
        
        size_t idx = 0;
        for (size_t i = 0; i < 100; ++i) {
            REQUIRE(arr[idx++] == i);
        }
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(arr[idx++] == i);
        }
        for (size_t i = 70000; i < 70100; ++i) {
            REQUIRE(arr[idx++] == i);
        }
    }

    TEST_CASE("ordering after random insertions") {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        
        size_t idx = 0;
        for (auto v : reference) {
            REQUIRE(arr[idx++] == v);
        }
    }

    TEST_CASE("ordering after mixed operations") {
        VebTree tree;
        std::set<size_t> reference;
        
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i);
            reference.insert(i);
        }
        
        for (size_t i = 10; i < 50; ++i) {
            tree.remove(i);
            reference.erase(i);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        
        size_t idx = 0;
        for (auto v : reference) {
            REQUIRE(arr[idx++] == v);
        }
    }

    TEST_CASE("iterator correctly positioned") {
        VebTree tree;
        for (size_t i = 0; i < 20; ++i) {
            tree.insert(i * 10);
        }
        
        auto it = tree.begin();
        REQUIRE(*it == 0);
        ++it;
        REQUIRE(*it == 10);
        ++it;
        REQUIRE(*it == 20);
    }

    TEST_CASE("iterator end condition") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        
        auto it = tree.begin();
        REQUIRE(*it == 1);
        ++it;
        REQUIRE(*it == 2);
        ++it;
        REQUIRE(*it == 3);
        ++it;
        REQUIRE(it == tree.end());
    }

    TEST_CASE("range-based for loop completes") {
        VebTree tree;
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i);
        }
        
        int count = 0;
        size_t prev = 0;
        for (auto v : tree) {
            REQUIRE(v >= prev);
            prev = v;
            count++;
        }
        REQUIRE(count == 100);
    }

    TEST_CASE("multiple iterations produce same result") {
        VebTree tree;
        for (size_t i = 0; i < 50; ++i) {
            tree.insert(i * 2);
        }
        
        std::vector<size_t> arr1(tree.begin(), tree.end());
        std::vector<size_t> arr2(tree.begin(), tree.end());
        
        REQUIRE(arr1 == arr2);
    }

    TEST_CASE("iterator consistency after clear") {
        VebTree tree;
        
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i);
        }
        
        tree.clear();
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.empty());
    }

    TEST_CASE("ordering with boundary values") {
        VebTree tree;
        tree.insert(0);
        tree.insert(1000000);
        tree.insert(500000);
        tree.insert(1);
        tree.insert(999999);
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr.size() == 5);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 1);
        REQUIRE(arr[2] == 500000);
        REQUIRE(arr[3] == 999999);
        REQUIRE(arr[4] == 1000000);
    }

    TEST_CASE("ordering with power of 2 boundaries") {
        VebTree tree;
        std::vector<size_t> vals{
            (1u << 8) - 1,
            (1u << 8),
            (1u << 8) + 1,
            (1u << 16) - 1,
            (1u << 16),
            (1u << 16) + 1
        };
        
        for (auto v : vals) {
            tree.insert(v);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        std::sort(vals.begin(), vals.end());
        
        REQUIRE(arr == vals);
    }

    TEST_CASE("large sparse set ordering") {
        VebTree tree;
        std::vector<size_t> vals;
        
        for (size_t i = 0; i < 10000; i += 100) {
            vals.push_back(i);
            tree.insert(i);
        }
        
        std::vector<size_t> arr(tree.begin(), tree.end());
        REQUIRE(arr == vals);
    }

    TEST_CASE("ordering after union operation") {
        VebTree s1;
        for (size_t i = 0; i < 50; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 25; i < 75; ++i) {
            s2.insert(i);
        }
        
        s1 |= s2;
        
        std::vector<size_t> arr(s1.begin(), s1.end());
        for (size_t i = 0; i < arr.size() - 1; ++i) {
            REQUIRE(arr[i] < arr[i + 1]);
        }
    }

    TEST_CASE("ordering after intersection operation") {
        VebTree s1;
        for (size_t i = 0; i < 100; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 25; i < 75; ++i) {
            s2.insert(i);
        }
        
        s1 &= s2;
        
        std::vector<size_t> arr(s1.begin(), s1.end());
        for (size_t i = 0; i < arr.size() - 1; ++i) {
            REQUIRE(arr[i] < arr[i + 1]);
        }
    }
}
