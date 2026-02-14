#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <string_view>

TEST_SUITE("VebTree Serialization") {
    TEST_CASE("serialize empty tree") {
        VebTree tree;
        auto serialized = tree.serialize();
        REQUIRE(!serialized.empty());
    }

    TEST_CASE("serialize and deserialize single element") {
        VebTree original;
        original.insert(42);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        REQUIRE(restored.size() == original.size());
        REQUIRE(restored.contains(42));
    }

    TEST_CASE("serialize and deserialize multiple elements") {
        VebTree original;
        original.insert(10);
        original.insert(20);
        original.insert(30);
        original.insert(100);
        original.insert(500);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        REQUIRE(restored.size() == original.size());
        REQUIRE(restored.contains(10));
        REQUIRE(restored.contains(20));
        REQUIRE(restored.contains(30));
        REQUIRE(restored.contains(100));
        REQUIRE(restored.contains(500));
    }

    TEST_CASE("deserialize and verify min/max") {
        VebTree original;
        original.insert(5);
        original.insert(15);
        original.insert(25);
        original.insert(35);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        REQUIRE(restored.min().value() == 5);
        REQUIRE(restored.max().value() == 35);
    }

    TEST_CASE("deserialize and verify successor/predecessor") {
        VebTree original;
        original.insert(10);
        original.insert(20);
        original.insert(30);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        auto succ = restored.successor(10);
        REQUIRE(succ.value() == 20);

        auto pred = restored.predecessor(20);
        REQUIRE(pred.value() == 10);
    }

    TEST_CASE("round-trip serialization") {
        VebTree original;
        for (size_t i = 0; i < 100; i += 5) {
            original.insert(i);
        }

        auto serialized1 = original.serialize();
        VebTree restored1 = VebTree::deserialize(std::string_view(serialized1));
        auto serialized2 = restored1.serialize();
        VebTree restored2 = VebTree::deserialize(std::string_view(serialized2));

        REQUIRE(restored2.size() == original.size());
        REQUIRE(restored2 == original);
    }

    TEST_CASE("serialize dense range") {
        VebTree original;
        for (size_t i = 1000; i < 1100; ++i) {
            original.insert(i);
        }

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        REQUIRE(restored.size() == 100);
        for (size_t i = 1000; i < 1100; ++i) {
            REQUIRE(restored.contains(i));
        }
    }

    TEST_CASE("serialize sparse range") {
        VebTree original;
        original.insert(0);
        original.insert(100000);
        original.insert(200000);
        original.insert(300000);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        REQUIRE(restored.size() == 4);
        REQUIRE(restored.contains(0));
        REQUIRE(restored.contains(100000));
        REQUIRE(restored.contains(200000));
        REQUIRE(restored.contains(300000));
    }

    TEST_CASE("deserialized tree supports all operations") {
        VebTree original;
        original.insert(10);
        original.insert(20);
        original.insert(30);

        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));

        restored.insert(40);
        REQUIRE(restored.size() == 4);
        REQUIRE(restored.contains(40));

        restored.remove(10);
        REQUIRE(restored.size() == 3);
        REQUIRE(!restored.contains(10));

        auto arr = std::vector<size_t>(restored.begin(), restored.end());
        REQUIRE(arr.size() == 3);
    }
}
