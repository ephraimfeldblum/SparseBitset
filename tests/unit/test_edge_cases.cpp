#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <random>

TEST_SUITE("VebTree Edge Cases & Stress Tests") {
    TEST_CASE("zero value") {
        VebTree tree;
        tree.insert(0);
        REQUIRE(tree.contains(0));
        REQUIRE(tree.size() == 1);
        REQUIRE(tree.min().value() == 0);
    }

    TEST_CASE("max long value") {
        VebTree tree;
        size_t max_val = std::numeric_limits<long>::max();
        tree.insert(max_val);
        REQUIRE(tree.contains(max_val));
        REQUIRE(tree.size() == 1);
    }

    TEST_CASE("dense small range") {
        VebTree tree;
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.size() == 100);
        for (size_t i = 0; i < 100; ++i) {
            REQUIRE(tree.contains(i));
        }
    }

    TEST_CASE("sparse large range") {
        VebTree tree;
        tree.insert(0);
        tree.insert(1000000);
        tree.insert(2000000);
        REQUIRE(tree.size() == 3);
        REQUIRE(tree.contains(0));
        REQUIRE(tree.contains(1000000));
        REQUIRE(tree.contains(2000000));
    }

    TEST_CASE("random insertions and removals") {
        VebTree tree;
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, 10000);

        std::vector<size_t> values;
        for (int i = 0; i < 100; ++i) {
            size_t val = dist(rng);
            tree.insert(val);
            values.push_back(val);
        }

        for (auto val : values) {
            REQUIRE(tree.contains(val));
        }

        for (int i = 0; i < 50; ++i) {
            tree.remove(values[i]);
        }

        for (int i = 0; i < 50; ++i) {
            REQUIRE(!tree.contains(values[i]));
        }
        for (int i = 50; i < 100; ++i) {
            REQUIRE(tree.contains(values[i]));
        }
    }

    TEST_CASE("successor/predecessor chain") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        tree.insert(40);
        tree.insert(50);

        auto current = tree.min();
        REQUIRE(current.value() == 10);

        current = tree.successor(current.value());
        REQUIRE(current.value() == 20);

        current = tree.successor(current.value());
        REQUIRE(current.value() == 30);

        current = tree.successor(current.value());
        REQUIRE(current.value() == 40);

        current = tree.successor(current.value());
        REQUIRE(current.value() == 50);

        current = tree.successor(current.value());
        REQUIRE(!current.has_value());
    }

    TEST_CASE("predecessor chain backward") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        tree.insert(40);
        tree.insert(50);

        auto current = tree.max();
        REQUIRE(current.value() == 50);

        current = tree.predecessor(current.value());
        REQUIRE(current.value() == 40);

        current = tree.predecessor(current.value());
        REQUIRE(current.value() == 30);

        current = tree.predecessor(current.value());
        REQUIRE(current.value() == 20);

        current = tree.predecessor(current.value());
        REQUIRE(current.value() == 10);

        current = tree.predecessor(current.value());
        REQUIRE(!current.has_value());
    }

    TEST_CASE("alternating insert/remove") {
        VebTree tree;
        for (size_t i = 0; i < 50; ++i) {
            tree.insert(i);
            tree.insert(i + 50);
        }
        REQUIRE(tree.size() == 100);

        for (size_t i = 0; i < 50; ++i) {
            tree.remove(i);
        }
        REQUIRE(tree.size() == 50);

        for (size_t i = 0; i < 50; ++i) {
            REQUIRE(!tree.contains(i));
            REQUIRE(tree.contains(i + 50));
        }
    }

    TEST_CASE("reinsert after removal") {
        VebTree tree;
        tree.insert(5);
        REQUIRE(tree.contains(5));

        tree.remove(5);
        REQUIRE(!tree.contains(5));

        tree.insert(5);
        REQUIRE(tree.contains(5));
    }

    TEST_CASE("large sequential insertions") {
        VebTree tree;
        const size_t count = 1000;
        for (size_t i = 0; i < count; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.size() == count);
        REQUIRE(tree.min().value() == 0);
        REQUIRE(tree.max().value() == count - 1);
    }

    TEST_CASE("large sequential removals") {
        VebTree tree;
        const size_t count = 1000;
        for (size_t i = 0; i < count; ++i) {
            tree.insert(i);
        }

        for (size_t i = 0; i < count; ++i) {
            tree.remove(i);
        }
        REQUIRE(tree.empty());
    }

    TEST_CASE("single element operations") {
        VebTree tree;
        tree.insert(42);
        REQUIRE(tree.size() == 1);
        REQUIRE(tree.min().value() == 42);
        REQUIRE(tree.max().value() == 42);
        REQUIRE(!tree.successor(42).has_value());
        REQUIRE(!tree.predecessor(42).has_value());
    }

    TEST_CASE("range query on single element") {
        VebTree tree;
        tree.insert(50);
        REQUIRE(tree.count_range(0, 49) == 0);
        REQUIRE(tree.count_range(50, 50) == 1);
        REQUIRE(tree.count_range(51, 100) == 0);
        REQUIRE(tree.count_range(0, 100) == 1);
    }

    TEST_CASE("min/max with single element") {
        VebTree tree;
        tree.insert(100);
        REQUIRE(tree.min().value() == 100);
        REQUIRE(tree.max().value() == 100);
    }

    TEST_CASE("min promotion after min removal") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        tree.insert(4);
        tree.insert(5);
        
        REQUIRE(tree.min().value() == 1);
        tree.remove(1);
        REQUIRE(tree.min().value() == 2);
        
        tree.remove(2);
        REQUIRE(tree.min().value() == 3);
    }

    TEST_CASE("max promotion after max removal") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        tree.insert(4);
        tree.insert(5);
        
        REQUIRE(tree.max().value() == 5);
        tree.remove(5);
        REQUIRE(tree.max().value() == 4);
        
        tree.remove(4);
        REQUIRE(tree.max().value() == 3);
    }

    TEST_CASE("min/max promotion with sparse elements") {
        VebTree tree;
        tree.insert(100);
        tree.insert(1000);
        tree.insert(10000);
        
        REQUIRE(tree.min().value() == 100);
        REQUIRE(tree.max().value() == 10000);
        
        tree.remove(100);
        REQUIRE(tree.min().value() == 1000);
        
        tree.remove(10000);
        REQUIRE(tree.max().value() == 1000);
    }

    TEST_CASE("min/max with two elements") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        
        REQUIRE(tree.min().value() == 10);
        REQUIRE(tree.max().value() == 20);
        
        tree.remove(10);
        REQUIRE(tree.min().value() == 20);
        REQUIRE(tree.max().value() == 20);
    }

    TEST_CASE("min/max after clear and reinsert") {
        VebTree tree;
        tree.insert(5);
        tree.insert(15);
        
        tree.clear();
        
        REQUIRE(!tree.min().has_value());
        REQUIRE(!tree.max().has_value());
        
        tree.insert(100);
        REQUIRE(tree.min().value() == 100);
        REQUIRE(tree.max().value() == 100);
    }

    TEST_CASE("min/max with boundary values") {
        VebTree tree;
        tree.insert(0);
        tree.insert(1000000);
        
        REQUIRE(tree.min().value() == 0);
        REQUIRE(tree.max().value() == 1000000);
    }

    TEST_CASE("min/max empty tree") {
        VebTree tree;
        
        REQUIRE(!tree.min().has_value());
        REQUIRE(!tree.max().has_value());
    }

    TEST_CASE("min/max with node type transitions") {
        VebTree tree;
        
        tree.insert(50);
        REQUIRE(tree.min().value() == 50);
        REQUIRE(tree.max().value() == 50);
        
        tree.insert(300);
        REQUIRE(tree.min().value() == 50);
        REQUIRE(tree.max().value() == 300);
        
        tree.insert(70000);
        REQUIRE(tree.min().value() == 50);
        REQUIRE(tree.max().value() == 70000);
        
        tree.remove(50);
        REQUIRE(tree.min().value() == 300);
        
        tree.remove(70000);
        REQUIRE(tree.max().value() == 300);
    }

    TEST_CASE("cascading min/max updates") {
        VebTree tree;
        for (size_t i = 10; i <= 20; ++i) {
            tree.insert(i);
        }
        
        REQUIRE(tree.min().value() == 10);
        REQUIRE(tree.max().value() == 20);
        
        for (size_t i = 10; i <= 19; ++i) {
            tree.remove(i);
            REQUIRE(tree.min().value() == i + 1);
        }
        
        REQUIRE(tree.min().value() == 20);
        REQUIRE(tree.max().value() == 20);
        
        tree.remove(20);
        REQUIRE(!tree.min().has_value());
        REQUIRE(!tree.max().has_value());
    }
}
