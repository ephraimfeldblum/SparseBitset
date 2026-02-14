#include "doctest.h"
#include "VEB/VebTree.hpp"

TEST_SUITE("Node16/32 Set Operation Edge Cases") {
    TEST_CASE("Node16 union with empty set") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 |= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 union of identical sets") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 |= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 union disjoint ranges") {
        VebTree s1;
        for (size_t i = 256; i < 384; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 384; i < 512; ++i) {
            s2.insert(i);
        }
        
        s1 |= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 union overlapping ranges") {
        VebTree s1;
        for (size_t i = 256; i < 400; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 350; i < 512; ++i) {
            s2.insert(i);
        }
        
        s1 |= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 intersection with empty set") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 &= s2;
        REQUIRE(s1.empty());
        REQUIRE(s1.size() == 0);
    }

    TEST_CASE("Node16 intersection of identical sets") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 &= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 intersection disjoint ranges") {
        VebTree s1;
        for (size_t i = 256; i < 384; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 384; i < 512; ++i) {
            s2.insert(i);
        }
        
        s1 &= s2;
        REQUIRE(s1.empty());
    }

    TEST_CASE("Node16 intersection overlapping ranges") {
        VebTree s1;
        for (size_t i = 256; i < 400; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 350; i < 450; ++i) {
            s2.insert(i);
        }
        
        s1 &= s2;
        REQUIRE(s1.size() == 50);
        for (size_t i = 350; i < 400; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 xor with empty set") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 ^= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 xor disjoint sets") {
        VebTree s1;
        for (size_t i = 256; i < 384; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 384; i < 512; ++i) {
            s2.insert(i);
        }
        
        s1 ^= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 256; i < 512; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node16 xor identical sets") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 ^= s2;
        REQUIRE(s1.empty());
    }

    TEST_CASE("Node32 union empty") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 |= s2;
        REQUIRE(s1.size() == 256);
    }

    TEST_CASE("Node32 union identical") {
        VebTree s1;
        for (size_t i = 65536; i < 66000; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 |= s2;
        REQUIRE(s1.size() == 464);
        for (size_t i = 65536; i < 66000; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node32 union disjoint") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 70000; i < 70256; ++i) {
            s2.insert(i);
        }
        
        s1 |= s2;
        REQUIRE(s1.size() == 512);
    }

    TEST_CASE("Node32 intersection empty") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 &= s2;
        REQUIRE(s1.empty());
    }

    TEST_CASE("Node32 intersection identical") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 &= s2;
        REQUIRE(s1.size() == 256);
    }

    TEST_CASE("Node32 intersection partial overlap") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 65664; i < 65920; ++i) {
            s2.insert(i);
        }
        
        s1 &= s2;
        REQUIRE(s1.size() == 128);
        for (size_t i = 65664; i < 65792; ++i) {
            REQUIRE(s1.contains(i));
        }
    }

    TEST_CASE("Node32 xor empty") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        
        s1 ^= s2;
        REQUIRE(s1.size() == 256);
    }

    TEST_CASE("Node32 xor identical") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2{s1};
        
        s1 ^= s2;
        REQUIRE(s1.empty());
    }

    TEST_CASE("Node32 xor disjoint") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 70000; i < 70256; ++i) {
            s2.insert(i);
        }
        
        s1 ^= s2;
        REQUIRE(s1.size() == 512);
    }

    TEST_CASE("Node32 xor partial overlap") {
        VebTree s1;
        for (size_t i = 65536; i < 65792; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 65664; i < 65920; ++i) {
            s2.insert(i);
        }
        
        s1 ^= s2;
        REQUIRE(s1.size() == 256);
        for (size_t i = 65536; i < 65664; ++i) {
            REQUIRE(s1.contains(i));
        }
        for (size_t i = 65792; i < 65920; ++i) {
            REQUIRE(s1.contains(i));
        }
        for (size_t i = 65664; i < 65792; ++i) {
            REQUIRE(!s1.contains(i));
        }
    }

    TEST_CASE("Node16 and Node32 mixed union") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 65536; i < 65792; ++i) {
            s2.insert(i);
        }
        
        s1 |= s2;
        REQUIRE(s1.size() == 512);
    }

    TEST_CASE("Node16 and Node32 mixed intersection") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 65536; i < 65792; ++i) {
            s2.insert(i);
        }
        
        s1 &= s2;
        REQUIRE(s1.empty());
    }

    TEST_CASE("single element set operations") {
        VebTree s1;
        s1.insert(500);
        
        VebTree s2;
        s2.insert(500);
        
        VebTree union_result{s1};
        union_result |= s2;
        REQUIRE(union_result.size() == 1);
        REQUIRE(union_result.contains(500));
        
        VebTree inter_result{s1};
        inter_result &= s2;
        REQUIRE(inter_result.size() == 1);
        REQUIRE(inter_result.contains(500));
        
        VebTree xor_result{s1};
        xor_result ^= s2;
        REQUIRE(xor_result.empty());
    }

    TEST_CASE("sequential set operations maintain correctness") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 384; i < 640; ++i) {
            s2.insert(i);
        }
        
        VebTree s3;
        for (size_t i = 0; i < 256; ++i) {
            s3.insert(i);
        }
        
        VebTree result{s1};
        result |= s2;
        result |= s3;
        
        for (size_t i = 0; i < 256; ++i) {
            REQUIRE(result.contains(i));
        }
        for (size_t i = 256; i < 640; ++i) {
            REQUIRE(result.contains(i));
        }
    }

    TEST_CASE("Node16 full dense range operations") {
        VebTree s1;
        for (size_t i = 256; i < 512; ++i) {
            s1.insert(i);
        }
        
        REQUIRE(s1.size() == 256);
        
        VebTree s2;
        for (size_t i = 256; i < 512; ++i) {
            s2.insert(i);
        }
        
        VebTree inter{s1};
        inter &= s2;
        REQUIRE(inter == s1);
    }

    TEST_CASE("Node32 sparse across multiple clusters") {
        VebTree s1;
        for (size_t i = 0; i < 100000; i += 1000) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 500; i < 100000; i += 1000) {
            s2.insert(i);
        }
        
        s1 |= s2;
        REQUIRE(s1.size() == 200);
        
        VebTree inter{s1};
        VebTree other;
        for (size_t i = 0; i < 100000; i += 2000) {
            other.insert(i);
        }
        inter &= other;
        REQUIRE(inter.size() == 50);
    }
}
