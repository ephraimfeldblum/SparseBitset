#include "doctest.h"
#include "VEB/VebTree.hpp"
#include <random>
#include <algorithm>

TEST_SUITE("VebTree Complex Scenarios") {
    TEST_CASE("walk forward and backward") {
        VebTree tree;
        std::vector<size_t> values = {10, 20, 30, 40, 50};
        for (auto v : values) {
            tree.insert(v);
        }
        
        std::vector<size_t> forward;
        for (auto v : tree) {
            forward.push_back(v);
        }
        REQUIRE(forward == values);
        
        std::vector<size_t> backward;
        for (auto it = tree.rbegin(); it != tree.rend(); --it) {
            backward.push_back(*it);
        }
        std::reverse(backward.begin(), backward.end());
        REQUIRE(backward == values);
    }

    TEST_CASE("successor chain equals iteration") {
        VebTree tree;
        for (size_t i = 0; i < 50; i += 5) {
            tree.insert(i);
        }
        
        std::vector<size_t> via_successor;
        auto curr = tree.min();
        while (curr.has_value()) {
            via_successor.push_back(curr.value());
            curr = tree.successor(curr.value());
        }
        
        std::vector<size_t> via_iteration;
        for (auto v : tree) {
            via_iteration.push_back(v);
        }
        
        REQUIRE(via_successor == via_iteration);
    }

    TEST_CASE("predecessor chain backward") {
        VebTree tree;
        for (size_t i = 100; i < 150; i += 5) {
            tree.insert(i);
        }
        
        std::vector<size_t> via_predecessor;
        auto curr = tree.max();
        while (curr.has_value()) {
            via_predecessor.push_back(curr.value());
            curr = tree.predecessor(curr.value());
        }
        std::reverse(via_predecessor.begin(), via_predecessor.end());
        
        std::vector<size_t> via_iteration;
        for (auto v : tree) {
            via_iteration.push_back(v);
        }
        
        REQUIRE(via_predecessor == via_iteration);
    }

    TEST_CASE("find gaps between elements") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        tree.insert(50);
        
        auto gap1 = tree.successor(10);
        REQUIRE(gap1.value() == 20);
        
        auto gap2 = tree.successor(20);
        REQUIRE(gap2.value() == 30);
        
        auto gap3 = tree.successor(30);
        REQUIRE(gap3.value() == 50);
        
        auto gap4 = tree.successor(50);
        REQUIRE(!gap4.has_value());
    }

    TEST_CASE("interleaved operations maintain invariants") {
        VebTree tree;
        
        tree.insert(5);
        REQUIRE(tree.min().value() == 5);
        REQUIRE(tree.max().value() == 5);
        
        tree.insert(15);
        REQUIRE(tree.min().value() == 5);
        REQUIRE(tree.max().value() == 15);
        
        tree.remove(5);
        REQUIRE(tree.min().value() == 15);
        REQUIRE(tree.max().value() == 15);
        
        tree.insert(10);
        REQUIRE(tree.min().value() == 10);
        REQUIRE(tree.max().value() == 15);
        
        tree.remove(15);
        REQUIRE(tree.min().value() == 10);
        REQUIRE(tree.max().value() == 10);
    }

    TEST_CASE("set operations preserve order") {
        VebTree s1;
        for (size_t i = 0; i < 10; ++i) {
            s1.insert(i * 2);
        }
        
        VebTree s2;
        for (size_t i = 0; i < 10; ++i) {
            s2.insert(i * 2 + 1);
        }
        
        s1 |= s2;
        
        auto arr = s1.to_vector();
        for (size_t i = 1; i < arr.size(); ++i) {
            REQUIRE(arr[i] > arr[i-1]);
        }
    }

    TEST_CASE("repeated operations are idempotent") {
        VebTree tree;
        tree.insert(10);
        tree.insert(10);
        tree.insert(10);
        REQUIRE(tree.size() == 1);
        REQUIRE(tree.contains(10));
    }

    TEST_CASE("remove nonexistent preserves state") {
        VebTree tree;
        tree.insert(5);
        tree.insert(10);
        tree.insert(15);
        
        tree.remove(7);
        tree.remove(12);
        tree.remove(99);
        
        REQUIRE(tree.size() == 3);
        REQUIRE(tree.contains(5));
        REQUIRE(tree.contains(10));
        REQUIRE(tree.contains(15));
    }

    TEST_CASE("alternating insert/remove maintains consistency") {
        VebTree tree;
        std::vector<size_t> inserted;
        
        for (size_t i = 0; i < 20; ++i) {
            tree.insert(i * 10);
            inserted.push_back(i * 10);
            
            if (i % 2 == 1 && i > 0) {
                size_t to_remove = inserted[i / 2];
                tree.remove(to_remove);
                inserted.erase(std::find(inserted.begin(), inserted.end(), to_remove));
            }
        }
        
        REQUIRE(tree.size() == inserted.size());
        for (auto v : inserted) {
            REQUIRE(tree.contains(v));
        }
    }

    TEST_CASE("count_range with various boundaries") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        tree.insert(40);
        tree.insert(50);
        
        REQUIRE(tree.count_range(5, 15) == 1);
        REQUIRE(tree.count_range(15, 35) == 2);
        REQUIRE(tree.count_range(35, 55) == 2);
        REQUIRE(tree.count_range(10, 50) == 5);
        REQUIRE(tree.count_range(0, 100) == 5);
        REQUIRE(tree.count_range(51, 100) == 0);
    }

    TEST_CASE("union then intersection") {
        VebTree s1;
        s1.insert(1); s1.insert(2); s1.insert(3);
        
        VebTree s2;
        s2.insert(2); s2.insert(3); s2.insert(4);
        
        VebTree s3;
        s3.insert(3); s3.insert(4); s3.insert(5);
        
        s1 |= s2;
        REQUIRE(s1.size() == 4);
        
        s1 &= s3;
        REQUIRE(s1.size() == 2);
        REQUIRE(s1.contains(3));
        REQUIRE(s1.contains(4));
    }

    TEST_CASE("xor is commutative") {
        VebTree s1;
        for (size_t i = 0; i < 10; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 5; i < 15; ++i) {
            s2.insert(i);
        }
        
        VebTree result1 = s1 ^ s2;

        VebTree result2 = s2 ^ s1;

        REQUIRE(result1 == result2);
    }

    TEST_CASE("de morgan's laws") {
        VebTree s1;
        for (size_t i = 0; i < 20; ++i) {
            s1.insert(i);
        }
        
        VebTree s2;
        for (size_t i = 10; i < 30; ++i) {
            s2.insert(i);
        }
        
        VebTree union_result = s1 | s2;
        
        VebTree intersection_result = s1 & s2;
        
        VebTree expected = s1 ^ s2;
        
        REQUIRE(expected.size() == union_result.size() - intersection_result.size());
    }

    TEST_CASE("serialized tree supports continued operations") {
        VebTree original;
        original.insert(10);
        original.insert(20);
        original.insert(30);
        
        auto serialized = original.serialize();
        VebTree restored = VebTree::deserialize(std::string_view(serialized));
        
        restored.insert(40);
        restored.insert(50);
        REQUIRE(restored.size() == 5);
        
        VebTree other;
        other.insert(5);
        other.insert(10);
        other.insert(15);
        
        restored &= other;
        REQUIRE(restored.size() == 1);
        REQUIRE(restored.contains(10));
    }

    TEST_CASE("copy is independent") {
        VebTree original;
        original.insert(10);
        original.insert(20);

        VebTree copy{original};
        copy.insert(30);

        REQUIRE(original.size() == 2);
        REQUIRE(!original.contains(30));
        REQUIRE(copy.size() == 3);
        REQUIRE(copy.contains(30));
    }

    TEST_CASE("move is transferring") {
        VebTree source;
        source.insert(10);
        source.insert(20);
        source.insert(30);
        
        VebTree dest = std::move(source);
        REQUIRE(dest.size() == 3);
        REQUIRE(dest.contains(10));
    }
}
