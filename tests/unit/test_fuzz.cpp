#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <random>
#include <set>

TEST_SUITE("VebTree Fuzz Tests") {
    TEST_CASE("fuzz random insertions and containment") {
        std::mt19937 rng(12345);
        std::uniform_int_distribution<size_t> dist(0, 100000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
        }
        
        REQUIRE(tree.size() == reference.size());
        
        for (auto val : reference) {
            REQUIRE(tree.contains(val));
        }
        
        for (size_t val = 0; val < 1000; ++val) {
            bool expected = reference.count(val) > 0;
            bool actual = tree.contains(val);
            REQUIRE(actual == expected);
        }
    }

    TEST_CASE("fuzz insertions and removals") {
        std::mt19937 rng(23456);
        std::uniform_int_distribution<size_t> dist(0, 50000);
        std::bernoulli_distribution insert_or_remove(0.7);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 1000; ++i) {
            size_t val = dist(rng);
            if (insert_or_remove(rng)) {
                tree.insert(val);
                reference.insert(val);
            } else {
                tree.remove(val);
                reference.erase(val);
            }
        }
        
        REQUIRE(tree.size() == reference.size());
        for (auto val : reference) {
            REQUIRE(tree.contains(val));
        }
    }

    TEST_CASE("fuzz successor/predecessor correctness") {
        std::mt19937 rng(34567);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 200; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
        }
        
        std::uniform_int_distribution<size_t> query_dist(0, 10000);
        for (int i = 0; i < 500; ++i) {
            size_t q = query_dist(rng);
            
            auto tree_succ = tree.successor(q);
            auto ref_it = reference.upper_bound(q);
            
            if (ref_it != reference.end()) {
                REQUIRE(tree_succ.has_value());
                REQUIRE(tree_succ.value() == *ref_it);
            } else {
                REQUIRE(!tree_succ.has_value());
            }
            
            auto tree_pred = tree.predecessor(q);
            auto ref_pred = reference.lower_bound(q);
            
            if (ref_pred != reference.begin()) {
                --ref_pred;
                REQUIRE(tree_pred.has_value());
                REQUIRE(tree_pred.value() == *ref_pred);
            } else {
                REQUIRE(!tree_pred.has_value());
            }
        }
    }

    TEST_CASE("fuzz union operations") {
        std::mt19937 rng(45678);
        std::uniform_int_distribution<size_t> dist(0, 5000);
        
        VebTree s1;
        std::set<size_t> ref1;
        for (int i = 0; i < 100; ++i) {
            size_t val = dist(rng);
            s1.insert(val);
            ref1.insert(val);
        }
        
        VebTree s2;
        std::set<size_t> ref2;
        for (int i = 0; i < 100; ++i) {
            size_t val = dist(rng);
            s2.insert(val);
            ref2.insert(val);
        }
        
        s1 |= s2;
        std::set<size_t> expected_union;
        std::set_union(ref1.begin(), ref1.end(), ref2.begin(), ref2.end(),
                      std::inserter(expected_union, expected_union.begin()));
        
        REQUIRE(s1.size() == expected_union.size());
        for (auto val : expected_union) {
            REQUIRE(s1.contains(val));
        }
    }

    TEST_CASE("fuzz intersection operations") {
        std::mt19937 rng(56789);
        std::uniform_int_distribution<size_t> dist(0, 5000);
        
        VebTree s1;
        std::set<size_t> ref1;
        for (int i = 0; i < 150; ++i) {
            size_t val = dist(rng);
            s1.insert(val);
            ref1.insert(val);
        }
        
        VebTree s2;
        std::set<size_t> ref2;
        for (int i = 0; i < 150; ++i) {
            size_t val = dist(rng);
            s2.insert(val);
            ref2.insert(val);
        }
        
        s1 &= s2;
        std::set<size_t> expected_inter;
        std::set_intersection(ref1.begin(), ref1.end(), ref2.begin(), ref2.end(),
                             std::inserter(expected_inter, expected_inter.begin()));
        
        REQUIRE(s1.size() == expected_inter.size());
        for (auto val : expected_inter) {
            REQUIRE(s1.contains(val));
        }
    }

    TEST_CASE("fuzz xor operations") {
        std::mt19937 rng(67890);
        std::uniform_int_distribution<size_t> dist(0, 5000);
        
        VebTree s1;
        std::set<size_t> ref1;
        for (int i = 0; i < 120; ++i) {
            size_t val = dist(rng);
            s1.insert(val);
            ref1.insert(val);
        }
        
        VebTree s2;
        std::set<size_t> ref2;
        for (int i = 0; i < 120; ++i) {
            size_t val = dist(rng);
            s2.insert(val);
            ref2.insert(val);
        }
        
        s1 ^= s2;
        std::set<size_t> expected_xor;
        std::set_symmetric_difference(ref1.begin(), ref1.end(), ref2.begin(), ref2.end(),
                                     std::inserter(expected_xor, expected_xor.begin()));
        
        REQUIRE(s1.size() == expected_xor.size());
        for (auto val : expected_xor) {
            REQUIRE(s1.contains(val));
        }
    }

    TEST_CASE("fuzz count_range accuracy") {
        std::mt19937 rng(78901);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 300; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
        }
        
        for (int i = 0; i < 100; ++i) {
            size_t start = dist(rng);
            size_t end = dist(rng);
            if (start > end) std::swap(start, end);
            
            auto tree_count = tree.count_range(start, end);
            auto ref_count = std::distance(reference.lower_bound(start), 
                                          reference.upper_bound(end));
            
            REQUIRE(tree_count == static_cast<size_t>(ref_count));
        }
    }

    TEST_CASE("fuzz min/max with dynamic insertions") {
        std::mt19937 rng(89012);
        std::uniform_int_distribution<size_t> dist(0, 100000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
            
            REQUIRE(tree.min().value() == *reference.begin());
            REQUIRE(tree.max().value() == *reference.rbegin());
        }
    }

    TEST_CASE("fuzz to_array ordering") {
        std::mt19937 rng(90123);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 250; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            reference.insert(val);
        }
        
        auto arr = std::vector<size_t>(tree.begin(), tree.end());
        REQUIRE(arr.size() == reference.size());
        
        size_t i = 0;
        for (auto val : reference) {
            REQUIRE(arr[i++] == val);
        }
    }

    TEST_CASE("fuzz iteration order matches to_array") {
        std::mt19937 rng(101234);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree tree;
        for (int i = 0; i < 200; ++i) {
            tree.insert(dist(rng));
        }
        
        auto arr = std::vector<size_t>(tree.begin(), tree.end());
        std::vector<size_t> via_iteration;
        for (auto val : tree) {
            via_iteration.push_back(val);
        }
        
        REQUIRE(arr == via_iteration);
    }

    TEST_CASE("fuzz empty/clear cycle") {
        std::mt19937 rng(112345);
        std::uniform_int_distribution<size_t> dist(0, 5000);
        
        VebTree tree;
        
        for (int cycle = 0; cycle < 20; ++cycle) {
            for (int i = 0; i < 50; ++i) {
                tree.insert(dist(rng));
            }
            REQUIRE(!tree.empty());
            
            tree.clear();
            REQUIRE(tree.empty());
            REQUIRE(tree.size() == 0);
        }
    }

    TEST_CASE("fuzz serialization round-trips") {
        std::mt19937 rng(123456);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree original;
        for (int i = 0; i < 200; ++i) {
            original.insert(dist(rng));
        }
        
        auto ser1 = original.serialize();
        VebTree restored1 = VebTree::deserialize(std::string_view(ser1));
        auto ser2 = restored1.serialize();
        VebTree restored2 = VebTree::deserialize(std::string_view(ser2));
        
        REQUIRE(original == restored2);
        REQUIRE(original.size() == restored2.size());
        for (auto val : original) {
            REQUIRE(restored2.contains(val));
        }
    }

    TEST_CASE("fuzz copy independence") {
        std::mt19937 rng(234567);
        std::uniform_int_distribution<size_t> dist(0, 10000);
        
        VebTree original;
        for (int i = 0; i < 100; ++i) {
            original.insert(dist(rng));
        }
        
        VebTree copy{original};
        
        for (int i = 0; i < 100; ++i) {
            size_t val = dist(rng);
            copy.insert(val);
            if (i % 10 == 0) {
                copy.remove(val);
            }
        }
        
        REQUIRE(copy != original);
    }

    TEST_CASE("fuzz all operations maintain size invariant") {
        std::mt19937 rng(345678);
        std::uniform_int_distribution<size_t> dist(0, 5000);
        std::bernoulli_distribution op_selector(0.33);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t val = dist(rng);
            int op = op_selector(rng) ? (op_selector(rng) ? 2 : 1) : 0;
            
            switch (op) {
                case 0: // insert
                    tree.insert(val);
                    reference.insert(val);
                    break;
                case 1: // remove
                    tree.remove(val);
                    reference.erase(val);
                    break;
                case 2: // contains (doesn't change size)
                    {
                        bool expected = reference.count(val) > 0;
                        bool actual = tree.contains(val);
                        REQUIRE(actual == expected);
                    }
                    break;
            }
            
            REQUIRE(tree.size() == reference.size());
        }
    }
}
