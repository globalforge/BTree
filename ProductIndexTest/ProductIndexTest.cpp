/*
 * ProductIndexTest.cpp - Unit tests for ProductIndex
 */

#include "ProductIndex.h"
#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>

// Simple test framework
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) runTest(#name, name)

static void runTest(const char* name, void (*testFunc)()) {
    testsRun++;
    std::cout << "Running: " << name << "... ";
    try {
        testFunc();
        testsPassed++;
        std::cout << "PASSED" << std::endl;
    }
    catch (const std::exception& e) {
        testsFailed++;
        std::cout << "FAILED: " << e.what() << std::endl;
    }
    catch (...) {
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

static void removeFile(const char* filename) {
    std::remove(filename);
}

// =============================================================================
// Tests
// =============================================================================

static TEST(test_create_and_build) {
    const char* filename = "test_create.dat";
    removeFile(filename);

    ProductIndex index;
    index.addRecord("ID001", 100);
    index.addRecord("ID002", 200);
    index.addRecord("ID003", 300);

    ASSERT_TRUE(index.buildIndex(filename));

    removeFile(filename);
}

static TEST(test_lookup) {
    const char* filename = "test_lookup.dat";
    removeFile(filename);

    {
        ProductIndex index;
        index.addRecord("apple", 111);
        index.addRecord("banana", 222);
        index.addRecord("cherry", 333);
        index.buildIndex(filename);
    }

    {
        ProductIndex index;
        ASSERT_TRUE(index.openIndex(filename));

        int64_t result;
        ASSERT_TRUE(index.lookup("apple", result));
        ASSERT_EQ(result, 111);

        ASSERT_TRUE(index.lookup("banana", result));
        ASSERT_EQ(result, 222);

        ASSERT_TRUE(index.lookup("cherry", result));
        ASSERT_EQ(result, 333);

        ASSERT_FALSE(index.lookup("durian", result));

        index.closeIndex();
    }

    removeFile(filename);
}

static TEST(test_contains_record) {
    const char* filename = "test_contains.dat";
    removeFile(filename);

    {
        ProductIndex index;
        index.addRecord("exists", 100);
        index.buildIndex(filename);
    }

    {
        ProductIndex index;
        index.openIndex(filename);

        ASSERT_TRUE(index.containsRecord("exists"));
        ASSERT_FALSE(index.containsRecord("notexists"));

        index.closeIndex();
    }

    removeFile(filename);
}

static TEST(test_many_records) {
    const char* filename = "test_many.dat";
    removeFile(filename);

    const int count = 10000;

    {
        ProductIndex index;
        for (int i = 0; i < count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "ID%08d", i);
            index.addRecord(key, static_cast<int64_t>(i * 100));
        }
        ASSERT_TRUE(index.buildIndex(filename));
    }

    {
        ProductIndex index;
        ASSERT_TRUE(index.openIndex(filename));
        ASSERT_EQ(index.getRecordCount(), static_cast<size_t>(count));

        // Spot check
        int64_t result;
        ASSERT_TRUE(index.lookup("ID00000000", result));
        ASSERT_EQ(result, 0);

        ASSERT_TRUE(index.lookup("ID00005000", result));
        ASSERT_EQ(result, 500000);

        ASSERT_TRUE(index.lookup("ID00009999", result));
        ASSERT_EQ(result, 999900);

        ASSERT_FALSE(index.lookup("ID00010000", result));

        index.closeIndex();
    }

    removeFile(filename);
}

static TEST(test_build_then_search_same_instance) {
    const char* filename = "test_same_instance.dat";
    removeFile(filename);

    ProductIndex index;
    index.addRecord("key1", 100);
    index.addRecord("key2", 200);

    // buildIndex should auto-open for reading
    ASSERT_TRUE(index.buildIndex(filename));

    // Should be able to search immediately
    int64_t result;
    ASSERT_TRUE(index.lookup("key1", result));
    ASSERT_EQ(result, 100);

    ASSERT_TRUE(index.lookup("key2", result));
    ASSERT_EQ(result, 200);

    index.closeIndex();
    removeFile(filename);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ProductIndex Unit Tests ===" << std::endl << std::endl;

    RUN_TEST(test_create_and_build);
    RUN_TEST(test_lookup);
    RUN_TEST(test_contains_record);
    RUN_TEST(test_many_records);
    RUN_TEST(test_build_then_search_same_instance);

    std::cout << std::endl << "=== Summary ===" << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    return (testsFailed > 0) ? 1 : 0;
}
