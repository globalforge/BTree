#pragma once

// MSVC compatibility
/*
#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
    #pragma warning(push)
    #pragma warning(disable: 4996)  // strncpy deprecation
#endif
*/

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

// Fixed-size record for disk storage
// Adjust MAX_PRODUCT_ID_LEN based on your actual maximum ID length
constexpr size_t MAX_PRODUCT_ID_LEN = 64;

#pragma pack(push, 1)
struct ProductRecord {
    char productId[MAX_PRODUCT_ID_LEN];
    int64_t nodeKey;
    
    ProductRecord();
    ProductRecord(const std::string& id, int64_t key);
    
    // For sorting
    bool operator<(const ProductRecord& other) const;
};
#pragma pack(pop)

class ProductIndex {
public:
    ProductIndex();
    ~ProductIndex();
    
    // --- Building the index ---
    
    // Add a record to the in-memory buffer (call during document load)
    void addRecord(const std::string& productId, int64_t nodeKey);
    
    // Sort and write all records to disk, then auto-open for reading
    // Returns false if write fails
    bool buildIndex(const std::string& indexFilePath);
    
    // --- Using the index ---
    
    // Open an existing index file for searching
    bool openIndex(const std::string& indexFilePath);
    
    // Close the index file
    void closeIndex();
    
    // Look up a node key by product ID
    // Returns true if found, false otherwise
    bool lookup(const std::string& productId, int64_t& outNodeKey);
    
    // Check if a product ID exists in the index
    bool containsRecord(const std::string& productId);
    
    // Get the number of records in the index
    size_t getRecordCount() const { return recordCount_; }
    
private:
    // For building
    std::vector<ProductRecord> buffer_;
    
    // For searching
    std::fstream indexFile_;
    size_t recordCount_;
    bool isOpen_;
    
    // Binary search implementation
    bool binarySearch(const char* targetId, int64_t& outNodeKey);
};

/*
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
*/
