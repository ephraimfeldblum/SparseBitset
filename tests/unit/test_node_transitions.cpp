#include "doctest.h"
#include "VEB/VebTree.hpp"

TEST_SUITE("VebTree Node Transitions") {
    TEST_CASE("Node8 range (< 256)") {
        VebTree tree;
        for (size_t i = 0; i < 256; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.size() == 256);
        REQUIRE(tree.min().value() == 0);
        REQUIRE(tree.max().value() == 255);
        
        for (size_t i = 0; i < 256; ++i) {
            REQUIRE(tree.contains(i));
        }
    }

    TEST_CASE("Node8 to Node16 transition") {
        VebTree tree;
        tree.insert(100);
        REQUIRE(tree.contains(100));
        
        tree.insert(256);
        REQUIRE(tree.contains(256));
        REQUIRE(tree.contains(100));
        REQUIRE(tree.size() == 2);
    }

    TEST_CASE("sparse values triggering Node16") {
        VebTree tree;
        tree.insert(10);
        tree.insert(1000);
        tree.insert(10000);
        
        REQUIRE(tree.size() == 3);
        REQUIRE(tree.contains(10));
        REQUIRE(tree.contains(1000));
        REQUIRE(tree.contains(10000));
    }

    TEST_CASE("Node16 range boundary") {
        VebTree tree;
        tree.insert(256);
        tree.insert(65535);
        
        REQUIRE(tree.size() == 2);
        REQUIRE(tree.contains(256));
        REQUIRE(tree.contains(65535));
    }

    TEST_CASE("Node16 to Node32 transition") {
        VebTree tree;
        tree.insert(1000);
        REQUIRE(tree.contains(1000));
        
        tree.insert(100000);
        REQUIRE(tree.contains(100000));
        REQUIRE(tree.contains(1000));
        REQUIRE(tree.size() == 2);
    }

    TEST_CASE("Node32 range values") {
        VebTree tree;
        tree.insert(65536);
        tree.insert(1000000);
        tree.insert(100000000);
        
        REQUIRE(tree.size() == 3);
        REQUIRE(tree.contains(65536));
        REQUIRE(tree.contains(1000000));
        REQUIRE(tree.contains(100000000));
    }

    TEST_CASE("Node32 boundary value") {
        VebTree tree;
        const size_t boundary = 4294967295UL;
        tree.insert(boundary);
        
        REQUIRE(tree.contains(boundary));
        REQUIRE(tree.size() == 1);
    }

    TEST_CASE("Node32 to Node64 transition") {
        VebTree tree;
        tree.insert(1000000000UL);
        tree.insert(10000000000UL);
        
        REQUIRE(tree.contains(1000000000UL));
        REQUIRE(tree.contains(10000000000UL));
        REQUIRE(tree.size() == 2);
    }

    TEST_CASE("Node64 large values") {
        VebTree tree;
        tree.insert(100000000000UL);
        tree.insert(1000000000000UL);
        
        REQUIRE(tree.size() == 2);
        REQUIRE(tree.contains(100000000000UL));
        REQUIRE(tree.contains(1000000000000UL));
    }

    TEST_CASE("mixed ranges all node types") {
        VebTree tree;
        tree.insert(10);           // Node8
        tree.insert(1000);         // Node16
        tree.insert(100000);       // Node32
        tree.insert(10000000000UL);// Node64
        
        REQUIRE(tree.size() == 4);
        REQUIRE(tree.min().value() == 10);
        REQUIRE(tree.max().value() == 10000000000UL);
        
        REQUIRE(tree.contains(10));
        REQUIRE(tree.contains(1000));
        REQUIRE(tree.contains(100000));
        REQUIRE(tree.contains(10000000000UL));
    }

    TEST_CASE("operations across node transitions") {
        VebTree tree;
        tree.insert(100);
        tree.insert(10000);
        tree.insert(1000000);
        
        auto succ = tree.successor(100);
        REQUIRE(succ.value() == 10000);
        
        succ = tree.successor(10000);
        REQUIRE(succ.value() == 1000000);
        
        auto pred = tree.predecessor(1000000);
        REQUIRE(pred.value() == 10000);
    }

    TEST_CASE("removal across node transitions") {
        VebTree tree;
        tree.insert(100);
        tree.insert(500000);
        
        tree.remove(100);
        REQUIRE(!tree.contains(100));
        REQUIRE(tree.contains(500000));
        
        tree.remove(500000);
        REQUIRE(tree.empty());
    }

    TEST_CASE("count_range across node boundaries") {
        VebTree tree;
        tree.insert(100);
        tree.insert(1000);
        tree.insert(100000);
        tree.insert(1000000);
        
        REQUIRE(tree.count_range(0, 500) == 1);
        REQUIRE(tree.count_range(500, 50000) == 1);
        REQUIRE(tree.count_range(50000, 2000000) == 2);
        REQUIRE(tree.count_range(0, 2000000) == 4);
    }

    TEST_CASE("universe_size reflects current node type") {
        VebTree tree;
        tree.insert(100);
        auto size1 = tree.universe_size();
        REQUIRE(size1 == 256);
        
        tree.insert(10000);
        auto size2 = tree.universe_size();
        REQUIRE(size2 == 65536);
        
        tree.insert(1000000);
        auto size3 = tree.universe_size();
        REQUIRE(size3 == 4294967296UL);
    }

    TEST_CASE("iterator preserves order across transitions") {
        VebTree tree;
        tree.insert(50);
        tree.insert(500);
        tree.insert(50000);
        tree.insert(5000000);
        
        auto arr = tree.to_vector();
        REQUIRE(arr.size() == 4);
        REQUIRE(arr[0] == 50);
        REQUIRE(arr[1] == 500);
        REQUIRE(arr[2] == 50000);
        REQUIRE(arr[3] == 5000000);
    }

    TEST_CASE("dense insertion in Node16 range") {
        VebTree tree;
        for (size_t i = 256; i < 512; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.size() == 256);
        REQUIRE(tree.min().value() == 256);
        REQUIRE(tree.max().value() == 511);
    }

    TEST_CASE("dense insertion in Node32 range") {
        VebTree tree;
        for (size_t i = 100000; i < 100256; ++i) {
            tree.insert(i);
        }
        REQUIRE(tree.size() == 256);
        REQUIRE(tree.min().value() == 100000);
        REQUIRE(tree.max().value() == 100255);
    }
}
