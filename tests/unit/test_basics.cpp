#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "VEB/VebTree.hpp"

TEST_SUITE("VebTree Basics") {
    TEST_CASE("create empty tree") {
        VebTree tree;
        REQUIRE(tree.empty());
        REQUIRE(tree.size() == 0);
    }

    TEST_CASE("insert single element") {
        VebTree tree;
        tree.insert(42);
        REQUIRE(!tree.empty());
        REQUIRE(tree.size() == 1);
        REQUIRE(tree.contains(42));
        REQUIRE(!tree.contains(41));
        REQUIRE(!tree.contains(43));
    }

    TEST_CASE("insert multiple elements") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        REQUIRE(tree.size() == 3);
        REQUIRE(tree.contains(10));
        REQUIRE(tree.contains(20));
        REQUIRE(tree.contains(30));
    }

    TEST_CASE("insert duplicate (idempotent)") {
        VebTree tree;
        tree.insert(5);
        tree.insert(5);
        REQUIRE(tree.size() == 1);
    }

    TEST_CASE("remove element") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.remove(10);
        REQUIRE(tree.size() == 1);
        REQUIRE(!tree.contains(10));
        REQUIRE(tree.contains(20));
    }

    TEST_CASE("remove nonexistent element") {
        VebTree tree;
        tree.insert(10);
        tree.remove(99);
        REQUIRE(tree.size() == 1);
        REQUIRE(tree.contains(10));
    }

    TEST_CASE("remove all elements") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        REQUIRE(tree.size() == 3);
        tree.remove(1);
        tree.remove(2);
        tree.remove(3);
        REQUIRE(tree.empty());
        REQUIRE(tree.size() == 0);
    }

    TEST_CASE("clear all elements") {
        VebTree tree;
        tree.insert(1);
        tree.insert(2);
        tree.insert(3);
        REQUIRE(tree.size() == 3);
        tree.clear();
        REQUIRE(tree.empty());
        REQUIRE(tree.size() == 0);
    }

    TEST_CASE("large range of values") {
        VebTree tree;
        const size_t large_val = 1000000;
        tree.insert(large_val);
        REQUIRE(tree.contains(large_val));
        REQUIRE(tree.size() == 1);
    }

    TEST_CASE("min and max") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(5);
        tree.insert(100);

        auto min = tree.min();
        REQUIRE(min.has_value());
        REQUIRE(min.value() == 5);

        auto max = tree.max();
        REQUIRE(max.has_value());
        REQUIRE(max.value() == 100);
    }

    TEST_CASE("min and max on empty tree") {
        VebTree tree;
        auto min = tree.min();
        REQUIRE(!min.has_value());

        auto max = tree.max();
        REQUIRE(!max.has_value());
    }

    TEST_CASE("move constructor") {
        VebTree tree1;
        tree1.insert(1);
        tree1.insert(2);
        tree1.insert(3);
        
        VebTree tree2 = std::move(tree1);
        REQUIRE(tree2.size() == 3);
        REQUIRE(tree2.contains(1));
        REQUIRE(tree2.contains(2));
        REQUIRE(tree2.contains(3));
    }

    TEST_CASE("move assignment") {
        VebTree tree1;
        tree1.insert(10);
        tree1.insert(20);

        VebTree tree2;
        tree2.insert(100);
        
        tree2 = std::move(tree1);
        REQUIRE(tree2.size() == 2);
        REQUIRE(tree2.contains(10));
        REQUIRE(tree2.contains(20));
        REQUIRE(!tree2.contains(100));
    }
}
