/*
    MIT License

    Copyright(c) 2025 Global Forge Computing

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files(the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions :

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "ProductIndex.h"
#include <algorithm>
#include <cstring>

// --- ProductRecord implementation ---

ProductRecord::ProductRecord() : nodeKey(0) {
    memset(productId, 0, MAX_PRODUCT_ID_LEN);
}

ProductRecord::ProductRecord(const std::string& id, int64_t key) : nodeKey(key) {
    memset(productId, 0, MAX_PRODUCT_ID_LEN);
    // Copy up to MAX_PRODUCT_ID_LEN - 1 characters to ensure null termination
    strncpy_s(productId, MAX_PRODUCT_ID_LEN, id.c_str(), MAX_PRODUCT_ID_LEN - 1);
}

bool ProductRecord::operator<(const ProductRecord& other) const {
    return strcmp(productId, other.productId) < 0;
}

// --- ProductIndex implementation ---

ProductIndex::ProductIndex() 
    : recordCount_(0)
    , isOpen_(false) {
}

ProductIndex::~ProductIndex() {
    closeIndex();
}

void ProductIndex::addRecord(const std::string& productId, int64_t nodeKey) {
    buffer_.emplace_back(productId, nodeKey);
}

bool ProductIndex::buildIndex(const std::string& indexFilePath) {
    if (buffer_.empty()) {
        return false;
    }
    
    // Sort records by product ID
    std::sort(buffer_.begin(), buffer_.end());
    
    // Write to file
    std::ofstream outFile(indexFilePath, std::ios::binary | std::ios::trunc);
    if (!outFile) {
        return false;
    }
    
    // Write record count as header
    size_t count = buffer_.size();
    outFile.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write all records
    outFile.write(
        reinterpret_cast<const char*>(buffer_.data()),
        buffer_.size() * sizeof(ProductRecord)
    );
    
    outFile.close();
    
    // Clear buffer to free memory
    buffer_.clear();
    buffer_.shrink_to_fit();
    
    // Auto-open for reading
    return openIndex(indexFilePath);
}

bool ProductIndex::openIndex(const std::string& indexFilePath) {
    closeIndex();
    
    indexFile_.open(indexFilePath, std::ios::binary | std::ios::in);
    if (!indexFile_) {
        return false;
    }
    
    // Read record count from header
    indexFile_.read(reinterpret_cast<char*>(&recordCount_), sizeof(recordCount_));
    if (!indexFile_) {
        closeIndex();
        return false;
    }
    
    isOpen_ = true;
    return true;
}

void ProductIndex::closeIndex() {
    if (indexFile_.is_open()) {
        indexFile_.close();
    }
    isOpen_ = false;
    recordCount_ = 0;
}

bool ProductIndex::lookup(const std::string& productId, int64_t& outNodeKey) {
    if (!isOpen_ || recordCount_ == 0) {
        return false;
    }
    
    // Prepare the search key (padded with nulls)
    char targetId[MAX_PRODUCT_ID_LEN];
    memset(targetId, 0, MAX_PRODUCT_ID_LEN);
    strncpy_s(targetId, MAX_PRODUCT_ID_LEN, productId.c_str(), MAX_PRODUCT_ID_LEN - 1);
    
    return binarySearch(targetId, outNodeKey);
}

bool ProductIndex::containsRecord(const std::string& productId) {
    int64_t unused;
    return lookup(productId, unused);
}

bool ProductIndex::binarySearch(const char* targetId, int64_t& outNodeKey) {
    // File layout: [size_t count][record 0][record 1]...[record n-1]
    const size_t headerSize = sizeof(size_t);
    const size_t recordSize = sizeof(ProductRecord);
    
    size_t left = 0;
    size_t right = recordCount_;
    ProductRecord record;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        
        // Seek to the middle record
        std::streampos pos = headerSize + (mid * recordSize);
        indexFile_.seekg(pos);
        
        // Read the record
        indexFile_.read(reinterpret_cast<char*>(&record), recordSize);
        if (!indexFile_) {
            return false;
        }
        
        int cmp = strcmp(targetId, record.productId);
        
        if (cmp == 0) {
            // Found it
            outNodeKey = record.nodeKey;
            return true;
        }
        else if (cmp < 0) {
            // Target is smaller, search left half
            right = mid;
        }
        else {
            // Target is larger, search right half
            left = mid + 1;
        }
    }
    
    return false; // Not found
}
