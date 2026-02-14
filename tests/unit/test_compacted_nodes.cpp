#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <set>
#include <algorithm>

TEST_SUITE("Compacted Node Clustering Behavior (Node16/32 Set Operations)") {
    TEST_CASE("Node16 OR compaction: two half-clusters merge to implicit full cluster") {
        VebTree a, b;
        
        size_t base_a = 1 * 256;
        size_t base_b = 3 * 256;
        size_t base_c = 5 * 256;
        
        a.insert(base_a);
        for (size_t i = 0; i < 128; ++i) {
            a.insert(base_b + i);
        }
        
        b.insert(base_c);
        for (size_t i = 128; i < 256; ++i) {
            b.insert(base_b + i);
        }
        
        VebTree dest = a | b;
        
        REQUIRE(dest.size() == 258);
        REQUIRE(dest.min().value() == base_a);
        REQUIRE(dest.max().value() == base_c);
        REQUIRE(dest.contains(base_b + 0));
        REQUIRE(dest.contains(base_b + 127));
        REQUIRE(dest.contains(base_b + 128));
        REQUIRE(dest.contains(base_b + 255));
        
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr.size() == 258);
    }

    TEST_CASE("Node16 or with compacted source") {
        VebTree src;
        VebTree other;
        
        size_t base_a = 1 * 256;
        size_t base_b = 3 * 256;
        size_t base_c = 5 * 256;
        
        src.insert(base_a);
        src.insert(base_c);
        for (size_t i = 0; i < 256; ++i) {
            src.insert(base_b + i);
        }
        
        other.insert(base_c + 1);
        other.insert(base_b + 13);
        other.insert(base_b + 37);
        
        VebTree dest = src | other;
        std::set<size_t> ref_src(src.begin(), src.end());
        std::set<size_t> ref_other(other.begin(), other.end());
        
        std::set<size_t> expected;
        std::set_union(ref_src.begin(), ref_src.end(), ref_other.begin(), ref_other.end(),
                      std::inserter(expected, expected.begin()));
        
        REQUIRE(dest.size() == expected.size());
    }

    TEST_CASE("Node16 and with compacted source") {
        VebTree src;
        VebTree other;
        
        size_t base_a = 1 * 256;
        size_t base_b = 3 * 256;
        size_t base_c = 5 * 256;
        
        src.insert(base_a);
        src.insert(base_c);
        src.insert(base_b);
        for (size_t i = 1; i < 256; ++i) {
            src.insert(base_b + i);
        }
        
        other.insert(base_c + 1);
        other.insert(base_b + 13);
        other.insert(base_b + 37);
        
        VebTree dest = src & other;
        
        REQUIRE(dest.contains(base_b + 13));
        REQUIRE(dest.contains(base_b + 37));
        REQUIRE(dest.size() == 2);
    }

    TEST_CASE("Node16 and compaction merge") {
        VebTree a;
        VebTree b;
        
        size_t base_a = 1 * 256;
        size_t base_b = 3 * 256;
        size_t base_c = 5 * 256;
        
        a.insert(base_a - 1);
        a.insert(base_a);
        a.insert(base_c - 1);
        a.insert(base_c);
        a.insert(base_b);
        for (size_t i = 1; i < 256; ++i) {
            a.insert(base_b + i);
        }
        
        b.insert(base_a);
        b.insert(base_a + 1);
        b.insert(base_c);
        b.insert(base_c + 1);
        b.insert(base_b);
        for (size_t i = 1; i < 256; ++i) {
            b.insert(base_b + i);
        }
        
        VebTree dest = a & b;
        REQUIRE(dest.size() == 258);
    }

    TEST_CASE("Node16 or compaction with min/max outside cluster") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 128; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        for (size_t i = 128; i < 256; ++i) {
            s2.insert(256 + i);
        }
        
        VebTree dest = s1 | s2;
        
        REQUIRE(dest.min().value() == 0);
        REQUIRE(dest.max().value() == 1000);
        REQUIRE(dest.contains(256 + 42));
        REQUIRE(dest.size() == 258);
    }

    TEST_CASE("Node16 and resident from nonresident") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        s2.insert(256);
        s2.insert(257);
        s2.insert(258);
        
        VebTree dest = s1 & s2;
        
        REQUIRE(dest.size() == 5);
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr.size() == 5);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 256);
        REQUIRE(arr[2] == 257);
        REQUIRE(arr[3] == 258);
        REQUIRE(arr[4] == 1000);
    }

    TEST_CASE("Node16 xor resident from nonresident") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        for (size_t i = 0; i < 10; ++i) {
            s2.insert(256 + i);
        }
        
        VebTree dest = s1 ^ s2;
        
        REQUIRE(dest.size() == 256 - 10);
    }

    TEST_CASE("Node16 or desync edge case") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(10000);
        s1.insert(266);
        s1.insert(532);
        
        s2.insert(0);
        s2.insert(10000);
        for (size_t i = 0; i < 256; ++i) {
            s2.insert(256 + i);
        }
        s2.insert(542);
        
        VebTree dest = s1 | s2;
        
        REQUIRE(dest.size() == 256 + 2 + 2);
        REQUIRE(dest.contains(532));
        REQUIRE(dest.contains(542));
        REQUIRE(dest.contains(266));
        REQUIRE(dest.min().value() == 0);
        REQUIRE(dest.max().value() == 10000);
    }

    TEST_CASE("Node16 xor full resident mix") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        s2.insert(256);
        s2.insert(257);
        s2.insert(258);
        
        VebTree dest = s1 ^ s2;
        
        REQUIRE(dest.size() == 253);
        REQUIRE(!dest.contains(256));
        REQUIRE(dest.contains(259));
        REQUIRE(dest.contains(511));
    }

    TEST_CASE("Node16 and full resident mix") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        s2.insert(256);
        s2.insert(257);
        s2.insert(258);
        
        VebTree dest = s1 & s2;
        
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr.size() == 5);
        REQUIRE(arr == std::vector<size_t>{0, 256, 257, 258, 1000});
    }

    TEST_CASE("Node16 or two full clusters") {
        VebTree s1;
        VebTree s2;
        
        s1.insert(0);
        s1.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s1.insert(256 + i);
        }
        
        s2.insert(0);
        s2.insert(1000);
        for (size_t i = 0; i < 256; ++i) {
            s2.insert(256 + i);
        }
        
        VebTree dest = s1 | s2;
        
        REQUIRE(dest.size() == 256 + 2);
    }

    TEST_CASE("Node32 and decompact") {
        VebTree key_full;
        VebTree key_partial;
        
        size_t N = 65536;
        size_t base_a = 1 * N;
        size_t base_b = 3 * N;
        size_t base_c = 5 * N;
        
        key_full.insert(base_a);
        key_full.insert(base_c);
        key_full.insert(base_b);
        
        for (size_t i = 1; i < N; ++i) {
            key_full.insert(base_b + i);
        }
        
        key_partial.insert(base_b + 10);
        key_partial.insert(base_b + 200);
        
        VebTree dest = key_full & key_partial;
        
        REQUIRE(dest.size() == 2);
        REQUIRE(dest.contains(base_b + 10));
        REQUIRE(dest.contains(base_b + 200));
        REQUIRE(dest.min().value() == base_b + 10);
        REQUIRE(dest.max().value() == base_b + 200);
    }

    TEST_CASE("Node32 or compaction merge") {
        VebTree a;
        VebTree b;
        
        size_t N = 65536;
        size_t base_a = 1 * N;
        size_t base_b = 3 * N;
        size_t base_c = 5 * N;
        
        a.insert(base_a);
        for (size_t i = 0; i < N / 2; ++i) {
            a.insert(base_b + i);
        }
        
        b.insert(base_c);
        for (size_t i = N / 2; i < N; ++i) {
            b.insert(base_b + i);
        }
        
        VebTree dest = a | b;
        
        REQUIRE(dest.size() == N + 2);
        REQUIRE(dest.contains(base_b + 42));
    }

    TEST_CASE("Node32 or with compacted source") {
        VebTree src;
        VebTree other;
        
        size_t N = 65536;
        size_t base_a = 1 * N;
        size_t base_b = 3 * N;
        size_t base_c = 5 * N;
        
        src.insert(base_a);
        src.insert(base_c);
        for (size_t i = 0; i < N; ++i) {
            src.insert(base_b + i);
        }
        
        other.insert(base_c + 1);
        other.insert(base_b + 13);
        other.insert(base_b + 37);
        
        VebTree dest = src | other;
        std::set<size_t> ref_src(src.begin(), src.end());
        std::set<size_t> ref_other(other.begin(), other.end());
        
        std::set<size_t> expected;
        std::set_union(ref_src.begin(), ref_src.end(), ref_other.begin(), ref_other.end(),
                      std::inserter(expected, expected.begin()));
        
        REQUIRE(dest.size() == expected.size());
    }

    TEST_CASE("Node32 and with compacted source") {
        VebTree src;
        VebTree other;
        
        size_t N = 65536;
        size_t base_a = 1 * N;
        size_t base_b = 3 * N;
        size_t base_c = 5 * N;
        
        src.insert(base_a);
        src.insert(base_c);
        for (size_t i = 0; i < N; ++i) {
            src.insert(base_b + i);
        }
        
        other.insert(base_c + 1);
        other.insert(base_b + 13);
        other.insert(base_b + 37);
        
        VebTree dest = src & other;
        
        REQUIRE(dest.size() == 2);
        REQUIRE(dest.contains(base_b + 13));
        REQUIRE(dest.contains(base_b + 37));
    }

    TEST_CASE("Node32 and compaction merge") {
        VebTree a;
        VebTree b;
        
        size_t N = 65536;
        size_t base_a = 1 * N;
        size_t base_b = 3 * N;
        size_t base_c = 5 * N;
        
        a.insert(base_a - 1);
        a.insert(base_a);
        a.insert(base_c - 1);
        a.insert(base_c);
        for (size_t i = 0; i < N; ++i) {
            a.insert(base_b + i);
        }
        
        b.insert(base_a);
        b.insert(base_a + 1);
        b.insert(base_c);
        b.insert(base_c + 1);
        for (size_t i = 0; i < N; ++i) {
            b.insert(base_b + i);
        }
        
        VebTree dest = a & b;
        
        REQUIRE(dest.size() == N + 2);
        REQUIRE(dest.contains(base_a));
        REQUIRE(dest.contains(base_c));
    }

    TEST_CASE("Node32 xor full resident mix") {
        VebTree s1;
        VebTree s2;
        
        size_t N = 65536;
        
        s1.insert(0);
        s1.insert(1000000);
        for (size_t i = 0; i < N; ++i) {
            s1.insert(N + i);
        }
        
        s2.insert(0);
        s2.insert(1000000);
        s2.insert(N);
        s2.insert(N + 1);
        s2.insert(N + 2);
        
        VebTree dest = s1 ^ s2;
        
        REQUIRE(!dest.contains(N));
        REQUIRE(dest.contains(N + 3));
        REQUIRE(dest.contains(2 * N - 1));
        REQUIRE(dest.size() == N - 3);
    }

    TEST_CASE("Node32 and full resident mix") {
        VebTree s1;
        VebTree s2;
        
        size_t N = 65536;
        
        s1.insert(0);
        s1.insert(1000000);
        for (size_t i = 0; i < N; ++i) {
            s1.insert(N + i);
        }
        
        s2.insert(0);
        s2.insert(1000000);
        s2.insert(N);
        s2.insert(N + 1);
        s2.insert(N + 2);
        
        VebTree dest = s1 & s2;
        
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr.size() == 5);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == N);
        REQUIRE(arr[2] == N + 1);
        REQUIRE(arr[3] == N + 2);
        REQUIRE(arr[4] == 1000000);
    }

    TEST_CASE("Node32 or two full clusters") {
        VebTree s1;
        VebTree s2;
        
        size_t N = 65536;
        
        s1.insert(0);
        s1.insert(1000000);
        for (size_t i = 0; i < N; ++i) {
            s1.insert(N + i);
        }
        
        s2.insert(0);
        s2.insert(1000000);
        for (size_t i = 0; i < N; ++i) {
            s2.insert(N + i);
        }
        
        VebTree dest = s1 | s2;
        
        REQUIRE(dest.size() == N + 2);
        REQUIRE(dest.min().value() == 0);
        REQUIRE(dest.max().value() == 1000000);
    }

    TEST_CASE("Node32 and promotion desync edge case") {
        VebTree s1;
        VebTree s2;
        
        size_t N = 65536;
        
        s1.insert(0);
        s1.insert(10000000);
        s1.insert(N + 10);
        s1.insert(2 * N + 20);
        
        s2.insert(0);
        s2.insert(10000000);
        s2.insert(2 * N + 20);
        
        VebTree dest = s1 & s2;
        
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr == std::vector<size_t>{0, 2 * N + 20, 10000000});
    }

    TEST_CASE("Node32 xor promotion desync edge case") {
        VebTree s1;
        VebTree s2;
        
        size_t N = 65536;
        
        s1.insert(0);
        s1.insert(10000000);
        s1.insert(N + 10);
        s1.insert(2 * N + 20);
        
        s2.insert(0);
        s2.insert(10000000);
        s2.insert(N + 10);
        s2.insert(2 * N + 30);
        
        VebTree dest = s1 ^ s2;
        
        std::vector<size_t> arr(dest.begin(), dest.end());
        REQUIRE(arr == std::vector<size_t>{2 * N + 20, 2 * N + 30});
    }
}
