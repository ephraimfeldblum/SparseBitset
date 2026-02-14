#include "doctest.h"
#include "VEB/VebTree.hpp"

TEST_SUITE("VebTree Query Operations") {
    TEST_CASE("successor of element") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);

        auto succ = tree.successor(5);
        REQUIRE(succ.has_value());
        REQUIRE(succ.value() == 10);

        succ = tree.successor(10);
        REQUIRE(succ.has_value());
        REQUIRE(succ.value() == 20);

        succ = tree.successor(25);
        REQUIRE(succ.has_value());
        REQUIRE(succ.value() == 30);
    }

    TEST_CASE("successor of max element") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);

        auto succ = tree.successor(30);
        REQUIRE(!succ.has_value());
    }

    TEST_CASE("successor on empty tree") {
        VebTree tree;
        auto succ = tree.successor(10);
        REQUIRE(!succ.has_value());
    }

    TEST_CASE("predecessor of element") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);

        auto pred = tree.predecessor(35);
        REQUIRE(pred.has_value());
        REQUIRE(pred.value() == 30);

        pred = tree.predecessor(30);
        REQUIRE(pred.has_value());
        REQUIRE(pred.value() == 20);

        pred = tree.predecessor(15);
        REQUIRE(pred.has_value());
        REQUIRE(pred.value() == 10);
    }

    TEST_CASE("predecessor of min element") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);

        auto pred = tree.predecessor(10);
        REQUIRE(!pred.has_value());
    }

    TEST_CASE("predecessor on empty tree") {
        VebTree tree;
        auto pred = tree.predecessor(10);
        REQUIRE(!pred.has_value());
    }

    TEST_CASE("count all elements") {
        VebTree tree;
        REQUIRE(tree.size() == 0);

        tree.insert(10);
        REQUIRE(tree.size() == 1);

        tree.insert(20);
        tree.insert(30);
        REQUIRE(tree.size() == 3);
    }

    TEST_CASE("count_range inclusive") {
        VebTree tree;
        tree.insert(5);
        tree.insert(10);
        tree.insert(15);
        tree.insert(20);
        tree.insert(25);

        REQUIRE(tree.count_range(5, 25) == 5);
        REQUIRE(tree.count_range(10, 20) == 3);
        REQUIRE(tree.count_range(6, 14) == 1);
        REQUIRE(tree.count_range(0, 4) == 0);
        REQUIRE(tree.count_range(30, 40) == 0);
    }

    TEST_CASE("interator") {
        VebTree tree;
        tree.insert(3);
        tree.insert(1);
        tree.insert(4);
        tree.insert(1);
        tree.insert(5);

        auto arr = std::vector<size_t>(tree.begin(), tree.end());
        REQUIRE(arr.size() == 4);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 3);
        REQUIRE(arr[2] == 4);
        REQUIRE(arr[3] == 5);
    }

    TEST_CASE("interator empty tree") {
        VebTree tree;
        auto arr = std::vector<size_t>(tree.begin(), tree.end());
        REQUIRE(arr.empty());
    }

    TEST_CASE("universe_size") {
        VebTree tree;
        tree.insert(10);
        REQUIRE(tree.universe_size() >= 11);

        tree.insert(1000);
        REQUIRE(tree.universe_size() >= 1001);
    }

    TEST_CASE("get_allocated_bytes") {
        VebTree tree;
        auto initial = tree.get_allocated_bytes();

        tree.insert(10);
        tree.insert(20);
        auto after_insert = tree.get_allocated_bytes();

        REQUIRE(after_insert >= initial);
    }

    TEST_CASE("memory stats") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);

        auto stats = tree.get_memory_stats();
        REQUIRE(stats.total_nodes > 0);
    }
}
