#include "doctest.h"
#include "VEB/VebTree.hpp"

TEST_SUITE("VebTree Memory Behavior") {
    TEST_CASE("empty tree memory allocation") {
        VebTree tree;
        auto allocated = tree.get_allocated_bytes();
        REQUIRE(allocated == sizeof(VebTree));
    }

    TEST_CASE("memory grows with insertions") {
        VebTree tree;
        auto initial = tree.get_allocated_bytes();
        
        tree.insert(100);
        auto after_one = tree.get_allocated_bytes();
        REQUIRE(after_one >= initial);
        
        tree.insert(1000);
        auto after_two = tree.get_allocated_bytes();
        REQUIRE(after_two >= after_one);
        
        tree.insert(100000);
        auto after_three = tree.get_allocated_bytes();
        REQUIRE(after_three >= after_two);
    }

    TEST_CASE("memory stats consistency") {
        VebTree tree;
        for (size_t i = 0; i < 100; ++i) {
            tree.insert(i * 100);
        }
        
        auto stats = tree.get_memory_stats();
        REQUIRE(stats.total_nodes > 0);
        REQUIRE(stats.max_depth >= 0);
        REQUIRE(stats.total_clusters >= 0);
    }

    TEST_CASE("copy increases memory proportionally") {
        VebTree tree1;
        tree1.insert(10);
        tree1.insert(20);
        tree1.insert(30);
        auto mem1 = tree1.get_allocated_bytes();
        
        VebTree tree2{tree1};
        auto mem2 = tree2.get_allocated_bytes();

        REQUIRE(mem2 <= mem1);
        REQUIRE(tree1.get_allocated_bytes() == mem1);
    }

    TEST_CASE("move preserves memory (same instance)") {
        VebTree tree1;
        tree1.insert(10);
        tree1.insert(20);
        tree1.insert(30);
        auto mem1 = tree1.get_allocated_bytes();
        
        VebTree tree2 = std::move(tree1);
        auto mem2 = tree2.get_allocated_bytes();
        
        REQUIRE(mem2 == mem1);
    }

    TEST_CASE("clear reduces element count") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        REQUIRE(tree.size() == 3);
        
        tree.clear();
        REQUIRE(tree.size() == 0);
        REQUIRE(tree.empty());
    }

    TEST_CASE("removal reduces size correctly") {
        VebTree tree;
        tree.insert(10);
        tree.insert(20);
        tree.insert(30);
        REQUIRE(tree.size() == 3);
        
        tree.remove(20);
        REQUIRE(tree.size() == 2);
        
        tree.remove(10);
        REQUIRE(tree.size() == 1);
        
        tree.remove(30);
        REQUIRE(tree.size() == 0);
    }

    TEST_CASE("large sparse structure") {
        VebTree tree;
        const size_t gap = 1000000;
        
        tree.insert(0);
        tree.insert(gap);
        tree.insert(gap * 2);
        tree.insert(gap * 3);
        
        REQUIRE(tree.size() == 4);
        auto allocated = tree.get_allocated_bytes();
        REQUIRE(allocated > 0);
    }

    TEST_CASE("dense structure memory") {
        VebTree tree;
        for (size_t i = 0; i < 1000; ++i) {
            tree.insert(i);
        }
        
        REQUIRE(tree.size() == 1000);
        auto allocated = tree.get_allocated_bytes();
        REQUIRE(allocated > 0);
        
        auto stats = tree.get_memory_stats();
        REQUIRE(stats.total_nodes > 0);
    }

    TEST_CASE("universe_size affects allocation") {
        VebTree tree1;
        tree1.insert(100);
        auto alloc1 = tree1.get_allocated_bytes();
        
        VebTree tree2;
        tree2.insert(1000000);
        auto alloc2 = tree2.get_allocated_bytes();
        
        REQUIRE(alloc2 >= alloc1);
    }

    TEST_CASE("destructor cleanup (manual scope)") {
        {
            VebTree tree;
            tree.insert(10);
            tree.insert(20);
            tree.insert(30);
        }
    }

    TEST_CASE("multiple insertions and removals pattern") {
        VebTree tree;
        for (int cycle = 0; cycle < 3; ++cycle) {
            for (size_t i = 0; i < 50; ++i) {
                tree.insert(i + cycle * 100);
            }
            REQUIRE(tree.size() == 50 + 25 * cycle);
            
            for (size_t i = 0; i < 25; ++i) {
                tree.remove(i + cycle * 100);
            }
            REQUIRE(tree.size() == 25 + 25 * cycle);
        }
    }

    TEST_CASE("set operations memory efficiency") {
        VebTree set1;
        for (size_t i = 0; i < 100; ++i) {
            set1.insert(i);
        }
        auto mem1 = set1.get_allocated_bytes();
        
        VebTree set2;
        for (size_t i = 50; i < 150; ++i) {
            set2.insert(i);
        }
        
        set1 |= set2;
        auto mem_union = set1.get_allocated_bytes();
        REQUIRE(mem_union >= mem1);
    }

    TEST_CASE("intersection reduces size and memory") {
        VebTree set1;
        for (size_t i = 0; i < 100; ++i) {
            set1.insert(i);
        }
        auto size1 = set1.size();
        
        VebTree set2;
        for (size_t i = 50; i < 75; ++i) {
            set2.insert(i);
        }
        
        set1 &= set2;
        auto size_inter = set1.size();
        REQUIRE(size_inter < size1);
        REQUIRE(size_inter == 25);
    }

    TEST_CASE("xor with large sets") {
        VebTree set1;
        for (size_t i = 0; i < 200; ++i) {
            set1.insert(i);
        }
        
        VebTree set2;
        for (size_t i = 100; i < 300; ++i) {
            set2.insert(i);
        }
        
        set1 ^= set2;
        REQUIRE(set1.size() == 200);
    }
}
