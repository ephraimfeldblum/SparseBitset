#include "doctest.h"
#include "VEB/VebTree.hpp"

TEST_SUITE("VebTree Set Operations") {
    TEST_CASE("equality") {
        VebTree tree1;
        tree1.insert(1);
        tree1.insert(2);
        tree1.insert(3);

        VebTree tree2;
        tree2.insert(1);
        tree2.insert(2);
        tree2.insert(3);

        REQUIRE(tree1 == tree2);

        tree2.insert(4);
        REQUIRE(!(tree1 == tree2));
    }

    TEST_CASE("inequality") {
        VebTree tree1;
        tree1.insert(1);
        tree1.insert(2);

        VebTree tree2;
        tree2.insert(1);
        tree2.insert(3);

        REQUIRE(tree1 != tree2);
    }

    TEST_CASE("union operation") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;
        set2.insert(3);
        set2.insert(4);
        set2.insert(5);

        set1 |= set2;

        REQUIRE(set1.size() == 5);
        REQUIRE(set1.contains(1));
        REQUIRE(set1.contains(2));
        REQUIRE(set1.contains(3));
        REQUIRE(set1.contains(4));
        REQUIRE(set1.contains(5));
    }

    TEST_CASE("intersection operation") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);
        set1.insert(4);

        VebTree set2;
        set2.insert(3);
        set2.insert(4);
        set2.insert(5);
        set2.insert(6);

        set1 &= set2;

        REQUIRE(set1.size() == 2);
        REQUIRE(set1.contains(3));
        REQUIRE(set1.contains(4));
        REQUIRE(!set1.contains(1));
        REQUIRE(!set1.contains(2));
        REQUIRE(!set1.contains(5));
    }

    TEST_CASE("intersection with no common elements") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;
        set2.insert(4);
        set2.insert(5);
        set2.insert(6);

        set1 &= set2;

        REQUIRE(set1.empty());
        REQUIRE(set1.size() == 0);
    }

    TEST_CASE("symmetric difference (XOR) operation") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;
        set2.insert(2);
        set2.insert(3);
        set2.insert(4);

        set1 ^= set2;

        REQUIRE(set1.size() == 2);
        REQUIRE(set1.contains(1));
        REQUIRE(set1.contains(4));
        REQUIRE(!set1.contains(2));
        REQUIRE(!set1.contains(3));
    }

    TEST_CASE("union with empty set") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;

        set1 |= set2;

        REQUIRE(set1.size() == 3);
        REQUIRE(set1.contains(1));
        REQUIRE(set1.contains(2));
        REQUIRE(set1.contains(3));
    }

    TEST_CASE("intersection with empty set") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;

        set1 &= set2;

        REQUIRE(set1.empty());
    }

    TEST_CASE("xor with empty set") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;

        set1 ^= set2;

        REQUIRE(set1.size() == 3);
        REQUIRE(set1.contains(1));
        REQUIRE(set1.contains(2));
        REQUIRE(set1.contains(3));
    }

    TEST_CASE("union is idempotent") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);

        VebTree result1{set1};
        result1 |= set1;

        REQUIRE(result1 == set1);
    }

    TEST_CASE("xor is self-inverse") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree set2;
        set2.insert(2);
        set2.insert(3);
        set2.insert(4);

        VebTree original{set1};

        set1 ^= set2;
        REQUIRE(set1.size() == 2);

        set1 ^= set2;
        REQUIRE(set1 == original);
    }

    TEST_CASE("intersection with self") {
        VebTree set1;
        set1.insert(1);
        set1.insert(2);
        set1.insert(3);

        VebTree result = set1 & set1;

        REQUIRE(result == set1);
    }

    TEST_CASE("union with destination as source") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);

        VebTree s2;
        s2.insert(3);
        s2.insert(4);
        s2.insert(5);

        s1 |= s1;
        REQUIRE(s1.size() == 3);
        REQUIRE(s1.contains(1));
        REQUIRE(s1.contains(2));
        REQUIRE(s1.contains(3));
    }

    TEST_CASE("union dest=s1 with s1 and s2") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);

        VebTree s2;
        s2.insert(3);
        s2.insert(4);
        s2.insert(5);

        s1 |= s2;
        REQUIRE(s1.size() == 5);
        REQUIRE(s1.contains(1));
        REQUIRE(s1.contains(2));
        REQUIRE(s1.contains(3));
        REQUIRE(s1.contains(4));
        REQUIRE(s1.contains(5));
    }

    TEST_CASE("intersection with destination as source") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);

        s1 &= s1;
        REQUIRE(s1.size() == 3);
    }

    TEST_CASE("intersection dest=s2 with s1 and s2") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);
        s1.insert(4);

        VebTree s2;
        s2.insert(3);
        s2.insert(4);
        s2.insert(5);
        s2.insert(6);

        s2 &= s1;
        REQUIRE(s2.size() == 2);
        REQUIRE(s2.contains(3));
        REQUIRE(s2.contains(4));
    }

    TEST_CASE("xor with destination as source") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);

        s1 ^= s1;
        REQUIRE(s1.empty());
        REQUIRE(s1.size() == 0);
    }

    TEST_CASE("xor dest=s1 with s1 and s2") {
        VebTree s1;
        s1.insert(1);
        s1.insert(2);
        s1.insert(3);

        VebTree s2;
        s2.insert(2);
        s2.insert(3);
        s2.insert(4);

        s1 ^= s2;
        REQUIRE(s1.size() == 2);
        REQUIRE(s1.contains(1));
        REQUIRE(s1.contains(4));
    }

    TEST_CASE("union with many sources") {
        std::vector<VebTree> sources(10);
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                sources[i].insert(i * 100 + j);
            }
        }

        VebTree result{sources[0]};
        for (int i = 1; i < 10; ++i) {
            result |= sources[i];
        }

        REQUIRE(result.size() == 100);
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                REQUIRE(result.contains(i * 100 + j));
            }
        }
    }

    TEST_CASE("intersection with many sources") {
        VebTree s1;
        for (int i = 0; i < 100; ++i) {
            s1.insert(i);
        }

        VebTree s2;
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0) s2.insert(i);
        }

        VebTree s3;
        for (int i = 0; i < 100; ++i) {
            if (i % 3 == 0) s3.insert(i);
        }

        s1 &= s2;
        s1 &= s3;

        REQUIRE(s1.size() == 17);
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0 && i % 3 == 0) {
                REQUIRE(s1.contains(i));
            } else {
                REQUIRE(!s1.contains(i));
            }
        }
    }

    TEST_CASE("node16 subset superset ops") {
        VebTree subset;
        for (size_t i = 256; i < 384; ++i) {
            subset.insert(i);
        }

        VebTree superset;
        for (size_t i = 256; i < 512; ++i) {
            superset.insert(i);
        }

        VebTree and_result = subset & superset;
        REQUIRE(and_result.size() == 128);

        VebTree or_result = subset | superset;
        REQUIRE(or_result.size() == 256);

        VebTree xor_result = subset ^ superset;
        REQUIRE(xor_result.size() == 128);
    }

    TEST_CASE("cross-node-type set operations") {
        VebTree s8;
        for (size_t i = 0; i < 100; ++i) {
            s8.insert(i);
        }

        VebTree s16;
        for (size_t i = 256; i < 512; ++i) {
            s16.insert(i);
        }

        VebTree s32;
        for (size_t i = 70000; i < 70100; ++i) {
            s32.insert(i);
        }

        VebTree union_result = s8 | s16;
        REQUIRE(union_result.size() == 100 + 256);

        union_result |= s32;
        REQUIRE(union_result.size() == 100 + 256 + 100);

        s8 &= s32;
        REQUIRE(s8.empty());
    }

    TEST_CASE("set ops with identical large sets") {
        VebTree s1;
        for (size_t i = 0; i < 10000; ++i) {
            s1.insert(i);
        }

        VebTree s2{s1};

        VebTree and_result = s1 & s2;
        REQUIRE(and_result == s1);

        VebTree or_result = s1 | s2;
        REQUIRE(or_result == s1);

        VebTree xor_result = s1 ^ s2;
        REQUIRE(xor_result.empty());
    }

    TEST_CASE("set ops commutativity") {
        VebTree s1;
        s1.insert(10);
        s1.insert(20);
        s1.insert(30);

        VebTree s2;
        s2.insert(20);
        s2.insert(30);
        s2.insert(40);

        VebTree s1_or_s2 = s1 | s2;
        VebTree s2_or_s1 = s2 | s1;
        REQUIRE(s1_or_s2 == s2_or_s1);

        s1 = VebTree();
        s1.insert(10);
        s1.insert(20);
        s1.insert(30);

        s2 = VebTree();
        s2.insert(20);
        s2.insert(30);
        s2.insert(40);

        s1_or_s2 = s1 & s2;
        s2_or_s1 = s2 & s1;
        REQUIRE(s1_or_s2 == s2_or_s1);
    }

    TEST_CASE("set ops associativity") {
        VebTree s1;
        for (size_t i = 0; i < 50; ++i) {
            s1.insert(i);
        }

        VebTree s2;
        for (size_t i = 25; i < 75; ++i) {
            s2.insert(i);
        }

        VebTree s3;
        for (size_t i = 50; i < 100; ++i) {
            s3.insert(i);
        }

        VebTree left_assoc = s1 | s2;
        left_assoc |= s3;

        VebTree right_assoc = s2 | s3;
        right_assoc = s1 | right_assoc;

        REQUIRE(left_assoc == right_assoc);
    }
}
