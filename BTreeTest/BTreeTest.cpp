/*
 * BTreeTest.cpp - Unit tests for the generic B-tree implementation
 * 
 * Compile: g++ -std=c++17 -O2 -o btree_test BTreeTest.cpp
 * Run:     ./btree_test
 */

#include "BTree.h"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <random>

// Simple test framework
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) runTest(#name, name)

void runTest(const char* name, void (*testFunc)()) {
    testsRun++;
    std::cout << "Running: " << name << "... ";
    try {
        testFunc();
        testsPassed++;
        std::cout << "PASSED" << std::endl;
    } catch (const std::exception& e) {
        testsFailed++;
        std::cout << "FAILED: " << e.what() << std::endl;
    } catch (...) {
        testsFailed++;
        std::cout << "FAILED: Unknown exception" << std::endl;
    }
}

#define ASSERT_TRUE(expr) \
    if (!(expr)) throw std::runtime_error("Assertion failed: " #expr)

#define ASSERT_FALSE(expr) \
    if (expr) throw std::runtime_error("Assertion failed: NOT " #expr)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)

#define ASSERT_STREQ(a, b) \
    if (std::strcmp((a), (b)) != 0) throw std::runtime_error("Assertion failed: " #a " == " #b)

// Cleanup helper
void removeFile(const char* filename) {
    std::remove(filename);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST(test_configuration_defaults) {
    // Default configuration: 32-byte key, 8-byte data, 4096-byte nodes
    using Tree = BTree<32, 8, 4096>;
    
    ASSERT_EQ(Tree::keySize(), 32u);
    ASSERT_EQ(Tree::dataSize(), 8u);
    ASSERT_TRUE(Tree::order() >= 3);  // Minimum order
    ASSERT_TRUE(Tree::maxKeys() >= 2);
    ASSERT_TRUE(Tree::minKeys() >= 1);
    
    std::cout << "(order=" << Tree::order() 
              << ", maxKeys=" << Tree::maxKeys() 
              << ", nodeSize=" << Tree::nodeSize() << ") ";
}

TEST(test_configuration_small_records) {
    // Small records should give higher order
    using Tree = BTree<8, 8, 4096>;
    
    ASSERT_EQ(Tree::keySize(), 8u);
    ASSERT_EQ(Tree::dataSize(), 8u);
    ASSERT_TRUE(Tree::order() > 50);  // Should be high with small records
    
    std::cout << "(order=" << Tree::order() << ") ";
}

TEST(test_configuration_large_records) {
    // Large records should give lower order
    using Tree = BTree<256, 256, 4096>;
    
    ASSERT_EQ(Tree::keySize(), 256u);
    ASSERT_EQ(Tree::dataSize(), 256u);
    ASSERT_TRUE(Tree::order() >= 3);  // Should still be at least 3
    
    std::cout << "(order=" << Tree::order() << ") ";
}

// =============================================================================
// Basic Operations Tests
// =============================================================================

TEST(test_create_empty_tree) {
    const char* filename = "test_empty.dat";
    removeFile(filename);
    
    BTree<32, sizeof(int64_t)> tree;
    ASSERT_FALSE(tree.isOpen());
    
    ASSERT_TRUE(tree.open(filename, BTreeMode::Write));
    ASSERT_TRUE(tree.isOpen());
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(tree.size(), 0);
    
    tree.close();
    ASSERT_FALSE(tree.isOpen());
    
    removeFile(filename);
}

TEST(test_insert_single) {
    const char* filename = "test_single.dat";
    removeFile(filename);
    
    BTree<32, sizeof(int64_t)> tree;
    tree.open(filename, BTreeMode::Write);
    
    int64_t data = 12345;
    tree.insertValue("key1", data);
    
    ASSERT_FALSE(tree.empty());
    ASSERT_EQ(tree.size(), 1);
    
    tree.close();
    removeFile(filename);
}

TEST(test_insert_and_retrieve) {
    const char* filename = "test_retrieve.dat";
    removeFile(filename);
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        
        tree.insertValue("apple", int64_t(111));
        tree.insertValue("banana", int64_t(222));
        tree.insertValue("cherry", int64_t(333));
        
        ASSERT_EQ(tree.size(), 3);
        tree.close();
    }
    
    // Reopen for reading
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        ASSERT_EQ(tree.size(), 3);
        
        int64_t result;
        
        ASSERT_TRUE(tree.retrieveValue("apple", result));
        ASSERT_EQ(result, 111);
        
        ASSERT_TRUE(tree.retrieveValue("banana", result));
        ASSERT_EQ(result, 222);
        
        ASSERT_TRUE(tree.retrieveValue("cherry", result));
        ASSERT_EQ(result, 333);
        
        ASSERT_FALSE(tree.retrieveValue("durian", result));
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_contains) {
    const char* filename = "test_contains.dat";
    removeFile(filename);
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        
        tree.insertValue("exists", int64_t(100));
        tree.close();
    }
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        ASSERT_TRUE(tree.contains("exists"));
        ASSERT_FALSE(tree.contains("notexists"));
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_string_keys) {
    const char* filename = "test_strings.dat";
    removeFile(filename);
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        
        std::string key = "mykey";
        tree.insertValue(key, int64_t(999));
        tree.close();
    }
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        std::string key = "mykey";
        int64_t result;
        ASSERT_TRUE(tree.retrieveValue(key, result));
        ASSERT_EQ(result, 999);
        
        ASSERT_TRUE(tree.contains(key));
        
        tree.close();
    }
    
    removeFile(filename);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(test_duplicate_key_throws) {
    const char* filename = "test_duplicate.dat";
    removeFile(filename);
    
    BTree<32, sizeof(int64_t)> tree;
    tree.open(filename, BTreeMode::Write);
    
    tree.insertValue("key", int64_t(100));
    
    bool threw = false;
    try {
        tree.insertValue("key", int64_t(200));  // Duplicate
    } catch (const std::runtime_error&) {
        threw = true;
    }
    
    ASSERT_TRUE(threw);
    
    tree.close();
    removeFile(filename);
}

TEST(test_retrieve_from_empty) {
    const char* filename = "test_empty_retrieve.dat";
    removeFile(filename);
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        tree.close();
    }
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        int64_t result;
        ASSERT_FALSE(tree.retrieveValue("anything", result));
        ASSERT_FALSE(tree.contains("anything"));
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_insert_requires_write_mode) {
    const char* filename = "test_write_mode.dat";
    removeFile(filename);
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        tree.insertValue("key", int64_t(1));
        tree.close();
    }
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        bool threw = false;
        try {
            tree.insertValue("key2", int64_t(2));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        ASSERT_TRUE(threw);
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_long_keys_truncated) {
    const char* filename = "test_long_key.dat";
    removeFile(filename);
    
    using Tree = BTree<8, sizeof(int64_t)>;  // Only 8 bytes for key (7 chars + null)
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Write);
        
        tree.insertValue("verylongkeythatwillbetruncated", int64_t(123));
        tree.close();
    }
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Read);
        
        int64_t result;
        // Should find it with truncated key
        ASSERT_TRUE(tree.retrieveValue("verylon", result));
        ASSERT_EQ(result, 123);
        
        // Original long key should also work (gets truncated to same value)
        ASSERT_TRUE(tree.retrieveValue("verylongkeythatwillbetruncated", result));
        
        tree.close();
    }
    
    removeFile(filename);
}

// =============================================================================
// Node Splitting Tests (insert enough to force splits)
// =============================================================================

TEST(test_node_splitting) {
    const char* filename = "test_splitting.dat";
    removeFile(filename);
    
    // Use small node size to force splits quickly
    using Tree = BTree<16, sizeof(int64_t), 256>;
    
    std::cout << "(order=" << Tree::order() << ") ";
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Write);
        
        // Insert enough items to force multiple splits
        for (int i = 0; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            tree.insertValue(key, int64_t(i));
        }
        
        ASSERT_EQ(tree.size(), 100);
        tree.close();
    }
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Read);
        
        ASSERT_EQ(tree.size(), 100);
        
        // Verify all items can be retrieved
        for (int i = 0; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            int64_t result;
            ASSERT_TRUE(tree.retrieveValue(key, result));
            ASSERT_EQ(result, i);
        }
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_reverse_insertion_order) {
    const char* filename = "test_reverse.dat";
    removeFile(filename);
    
    using Tree = BTree<16, sizeof(int64_t), 256>;
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Write);
        
        // Insert in reverse order
        for (int i = 99; i >= 0; i--) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            tree.insertValue(key, int64_t(i));
        }
        
        tree.close();
    }
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Read);
        
        // Verify all items
        for (int i = 0; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            int64_t result;
            ASSERT_TRUE(tree.retrieveValue(key, result));
            ASSERT_EQ(result, i);
        }
        
        tree.close();
    }
    
    removeFile(filename);
}

TEST(test_random_insertion_order) {
    const char* filename = "test_random.dat";
    removeFile(filename);
    
    using Tree = BTree<16, sizeof(int64_t), 256>;
    
    // Create shuffled indices
    std::vector<int> indices(100);
    for (int i = 0; i < 100; i++) indices[i] = i;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Write);
        
        for (int i : indices) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            tree.insertValue(key, int64_t(i));
        }
        
        tree.close();
    }
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Read);
        
        for (int i = 0; i < 100; i++) {
            char key[16];
            snprintf(key, sizeof(key), "key%05d", i);
            int64_t result;
            ASSERT_TRUE(tree.retrieveValue(key, result));
            ASSERT_EQ(result, i);
        }
        
        tree.close();
    }
    
    removeFile(filename);
}

// =============================================================================
// Larger Scale Test
// =============================================================================

TEST(test_thousands_of_records) {
    const char* filename = "test_thousands.dat";
    removeFile(filename);
    
    const int count = 10000;
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Write);
        
        for (int i = 0; i < count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "ID%08d", i);
            tree.insertValue(key, int64_t(i * 100));
        }
        
        ASSERT_EQ(tree.size(), count);
        tree.close();
    }
    
    {
        BTree<32, sizeof(int64_t)> tree;
        tree.open(filename, BTreeMode::Read);
        
        ASSERT_EQ(tree.size(), count);
        
        // Spot check some values
        int64_t result;
        
        ASSERT_TRUE(tree.retrieveValue("ID00000000", result));
        ASSERT_EQ(result, 0);
        
        ASSERT_TRUE(tree.retrieveValue("ID00005000", result));
        ASSERT_EQ(result, 500000);
        
        ASSERT_TRUE(tree.retrieveValue("ID00009999", result));
        ASSERT_EQ(result, 999900);
        
        ASSERT_FALSE(tree.retrieveValue("ID00010000", result));
        
        tree.close();
    }
    
    removeFile(filename);
}

// =============================================================================
// Data Type Tests
// =============================================================================

TEST(test_struct_data) {
    const char* filename = "test_struct.dat";
    removeFile(filename);
    
    struct MyData {
        double value;
        int count;
        char label[20];
    };
    
    using Tree = BTree<32, sizeof(MyData)>;
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Write);
        
        MyData data1 = {3.14159, 42, "hello"};
        MyData data2 = {2.71828, 99, "world"};
        
        tree.insert("first", &data1);
        tree.insert("second", &data2);
        
        tree.close();
    }
    
    {
        Tree tree;
        tree.open(filename, BTreeMode::Read);
        
        MyData result;
        
        ASSERT_TRUE(tree.retrieve("first", &result));
        ASSERT_EQ(result.count, 42);
        ASSERT_STREQ(result.label, "hello");
        
        ASSERT_TRUE(tree.retrieve("second", &result));
        ASSERT_EQ(result.count, 99);
        ASSERT_STREQ(result.label, "world");
        
        tree.close();
    }
    
    removeFile(filename);
}

// =============================================================================
// Move Semantics Test
// =============================================================================

TEST(test_move_semantics) {
    const char* filename = "test_move.dat";
    removeFile(filename);
    
    BTree<32, sizeof(int64_t)> tree1;
    tree1.open(filename, BTreeMode::Write);
    
    tree1.insertValue("key", int64_t(123));
    
    // Move construct
    BTree<32, sizeof(int64_t)> tree2 = std::move(tree1);
    
    ASSERT_TRUE(tree2.isOpen());
    ASSERT_FALSE(tree1.isOpen());
    ASSERT_EQ(tree2.size(), 1);
    
    tree2.close();
    removeFile(filename);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== BTree Unit Tests ===" << std::endl << std::endl;
    
    // Configuration tests
    RUN_TEST(test_configuration_defaults);
    RUN_TEST(test_configuration_small_records);
    RUN_TEST(test_configuration_large_records);
    
    std::cout << std::endl;
    
    // Basic operations
    RUN_TEST(test_create_empty_tree);
    RUN_TEST(test_insert_single);
    RUN_TEST(test_insert_and_retrieve);
    RUN_TEST(test_contains);
    RUN_TEST(test_string_keys);
    
    std::cout << std::endl;
    
    // Edge cases
    RUN_TEST(test_duplicate_key_throws);
    RUN_TEST(test_retrieve_from_empty);
    RUN_TEST(test_insert_requires_write_mode);
    RUN_TEST(test_long_keys_truncated);
    
    std::cout << std::endl;
    
    // Node splitting
    RUN_TEST(test_node_splitting);
    RUN_TEST(test_reverse_insertion_order);
    RUN_TEST(test_random_insertion_order);
    
    std::cout << std::endl;
    
    // Scale
    RUN_TEST(test_thousands_of_records);
    
    std::cout << std::endl;
    
    // Data types
    RUN_TEST(test_struct_data);
    
    std::cout << std::endl;
    
    // Move semantics
    RUN_TEST(test_move_semantics);
    
    // Summary
    std::cout << std::endl << "=== Summary ===" << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    
    return (testsFailed > 0) ? 1 : 0;
}
