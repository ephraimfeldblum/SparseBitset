#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <random>
#include <set>

TEST_SUITE("VebTree Count Range") {
    TEST_CASE("count_range basic") {
        VebTree tree;
        for (size_t i = 0; i < 200; ++i) {
            tree.insert(i);
        }
        
        REQUIRE(tree.count_range(10, 20) == 11);
        REQUIRE(tree.count_range(0, 0) == 1);
        REQUIRE(tree.count_range(199, 300) == 1);
        REQUIRE(tree.count_range(0, 1000) == 200);
    }

    TEST_CASE("count_range no matches") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        
        REQUIRE(tree.count_range(40, 50) == 0);
        REQUIRE(tree.count_range(11, 19) == 0);
        REQUIRE(tree.count_range(100, 200) == 0);
    }

    TEST_CASE("count_range single element") {
        VebTree tree;
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i * 10);
        }
        
        REQUIRE(tree.count_range(50, 50) == 1);
        REQUIRE(tree.count_range(0, 0) == 1);
        REQUIRE(tree.count_range(990, 990) == 1);
    }

    TEST_CASE("count_range promote min on delete") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        tree.insert(1000);
        
        REQUIRE(tree.count_range(1, 1000) == 4);
        
        tree.remove(1);
        REQUIRE(tree.count_range(1, 1) == 0);
        REQUIRE(tree.count_range(2, 3) == 2);
        REQUIRE(tree.count_range(2, 1000) == 3);
        
        tree.remove(2);
        REQUIRE(tree.count_range(2, 1000) == 2);
        REQUIRE(tree.count_range(3, 1000) == 2);
    }

    TEST_CASE("count_range promote max on delete") {
        VebTree tree;
        tree.insert(0);
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        tree.insert(4);
        
        REQUIRE(tree.count_range(0, 4) == 5);
        
        tree.remove(4);
        REQUIRE(tree.count_range(3, 4) == 1);
        
        tree.remove(3);
        REQUIRE(tree.count_range(0, 10) == 3);
        REQUIRE(tree.count_range(3, 10) == 0);
    }

    TEST_CASE("count_range cross cluster boundaries") {
        VebTree tree;
        std::vector<size_t> pts{0, 15, 16, 17, 255, 256, 257, 1023, 1024, 1025};
        for (auto p : pts) {
            tree.insert(p);
        }
        
        REQUIRE(tree.count_range(15, 16) == 2);
        REQUIRE(tree.count_range(16, 256) == 6);
        REQUIRE(tree.count_range(0, 1025) == 10);
        
        tree.remove(16);
        tree.remove(256);
        REQUIRE(tree.count_range(15, 256) == 7);
        REQUIRE(tree.count_range(256, 1025) == 3);
    }

    TEST_CASE("count_range randomized small") {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> val_dist(0, 2000);
        std::uniform_real_distribution<> op_dist(0.0, 1.0);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 300; ++i) {
            size_t v = val_dist(rng);
            if (op_dist(rng) < 0.6) {
                tree.insert(v);
                reference.insert(v);
            } else {
                tree.remove(v);
                reference.erase(v);
            }
        }
        
        for (int i = 0; i < 200; ++i) {
            size_t a = val_dist(rng);
            size_t b = val_dist(rng);
            size_t lo = std::min(a, b);
            size_t hi = std::max(a, b);
            
            auto ref_count = std::distance(reference.lower_bound(lo), 
                                          reference.upper_bound(hi));
            REQUIRE(tree.count_range(lo, hi) == static_cast<size_t>(ref_count));
        }
    }

    TEST_CASE("count_range randomized large universe") {
        std::mt19937 rng(123);
        std::uniform_int_distribution<size_t> val_dist(0, 1000000);
        std::uniform_real_distribution<> op_dist(0.0, 1.0);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t v = val_dist(rng);
            if (op_dist(rng) < 0.55) {
                tree.insert(v);
                reference.insert(v);
            } else {
                tree.remove(v);
                reference.erase(v);
            }
        }
        
        for (int i = 0; i < 150; ++i) {
            size_t a = val_dist(rng);
            size_t b = val_dist(rng);
            size_t lo = std::min(a, b);
            size_t hi = std::max(a, b);
            
            auto ref_count = std::distance(reference.lower_bound(lo), 
                                          reference.upper_bound(hi));
            REQUIRE(tree.count_range(lo, hi) == static_cast<size_t>(ref_count));
        }
    }

    TEST_CASE("count_range dense ranges") {
        VebTree tree;
        for (size_t i = 0; i < 1000; ++i) {
            tree.insert(i);
        }
        
        REQUIRE(tree.count_range(0, 999) == 1000);
        REQUIRE(tree.count_range(100, 199) == 100);
        REQUIRE(tree.count_range(500, 600) == 101);
        REQUIRE(tree.count_range(250, 250) == 1);
        REQUIRE(tree.count_range(750, 749) == 0);
    }

    TEST_CASE("count_range sparse ranges") {
        VebTree tree;
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i * 100);
        }
        
        REQUIRE(tree.count_range(0, 9999) == 100);
        REQUIRE(tree.count_range(1000, 2000) == 11);
        REQUIRE(tree.count_range(5000, 5000) == 1);
        REQUIRE(tree.count_range(5001, 5099) == 0);
    }

    TEST_CASE("count_range boundary conditions") {
        VebTree tree;
        tree.insert(0);
        tree.insert(1000000);
        
        REQUIRE(tree.count_range(0, 0) == 1);
        REQUIRE(tree.count_range(1000000, 1000000) == 1);
        REQUIRE(tree.count_range(0, 1000000) == 2);
        REQUIRE(tree.count_range(1, 999999) == 0);
    }

    TEST_CASE("count_range all nodes in range") {
        VebTree tree;
        std::vector<size_t> elements{100, 500, 1000, 50000, 100000};
        for (auto e : elements) {
            tree.insert(e);
        }
        
        REQUIRE(tree.count_range(0, 200000) == 5);
        REQUIRE(tree.count_range(100, 100000) == 5);
        REQUIRE(tree.count_range(101, 100000) == 4);
    }

    TEST_CASE("count_range interleaved ops and queries") {
        VebTree tree;
        std::set<size_t> reference;
        
        tree.insert(10);
        reference.insert(10);
        REQUIRE(tree.count_range(5, 15) == 1);
        
        tree.insert(20);
        reference.insert(20);
        REQUIRE(tree.count_range(5, 25) == 2);
        
        tree.remove(10);
        reference.erase(10);
        REQUIRE(tree.count_range(5, 25) == 1);
        REQUIRE(tree.count_range(5, 15) == 0);
        
        for (size_t i = 30; i < 50; ++i) {
            tree.insert(i);
            reference.insert(i);
        }
        
        REQUIRE(tree.count_range(5, 50) == 21);
        REQUIRE(tree.count_range(25, 45) == 20);
    }

    TEST_CASE("count_range after clear and re-insert") {
        VebTree tree;
        
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.count_range(0, 99) == 100);
        
        tree.clear();
        REQUIRE(tree.count_range(0, 99) == 0);
        
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i * 2);
        }
        REQUIRE(tree.count_range(0, 200) == 100);
        REQUIRE(tree.count_range(50, 150) == 51);
    }

    TEST_CASE("count_range with node32 values") {
        VebTree tree;
        std::vector<size_t> vals{100, 70000, 100000, 1000000, 2000000};
        std::set<size_t> reference(vals.begin(), vals.end());
        
        for (auto v : vals) {
            tree.insert(v);
        }
        
        REQUIRE(tree.count_range(0, 3000000) == 5);
        REQUIRE(tree.count_range(50000, 2000000) == 5);
        REQUIRE(tree.count_range(70001, 999999) == 2);
        REQUIRE(tree.count_range(2000001, 3000000) == 0);
    }

    TEST_CASE("count_range stress test 500 elements") {
        std::mt19937 rng(999);
        std::uniform_int_distribution<size_t> val_dist(0, 100000);
        
        VebTree tree;
        std::set<size_t> reference;
        
        for (int i = 0; i < 500; ++i) {
            size_t v = val_dist(rng);
            tree.insert(v);
            reference.insert(v);
        }
        
        for (int i = 0; i < 300; ++i) {
            size_t a = val_dist(rng);
            size_t b = val_dist(rng);
            size_t lo = std::min(a, b);
            size_t hi = std::max(a, b);
            
            auto ref_count = std::distance(reference.lower_bound(lo), 
                                          reference.upper_bound(hi));
            auto tree_count = tree.count_range(lo, hi);
            REQUIRE(tree_count == static_cast<size_t>(ref_count));
        }
    }
}
