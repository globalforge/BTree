# ProductIndex

A high-performance, file-based index for mapping product IDs to node keys in large CAD/PLM assemblies. Designed for integration with Siemens Vis Mockup and similar applications where product structures can contain millions of parts.

## Problem

When working with large product structures (e.g., an entire submarine with millions of parts), selecting a node by product ID requires traversing the entire tree. This can take **hours** for very large assemblies.

Common solutions have trade-offs:
- **In-memory hash table**: Fast lookups but requires gigabytes of RAM
- **Database**: Adds deployment complexity and external dependencies

## Solution

ProductIndex uses a **sorted file with binary search** to provide:
- O(log n) lookup performance
- Minimal memory footprint (only one record in memory at a time)
- Single-file deployment (no external dependencies)
- Sub-millisecond lookups even with millions of records

## Performance

Tested with 2 million records on a standard workstation:

| Metric | Value |
|--------|-------|
| Index build time | ~660 ms |
| Index file size | 138 MB |
| Average lookup | **195 μs** |
| Worst case lookup | ~450 μs |
| Memory usage | Negligible |

## Usage

### Building the Index

During document load, traverse your product structure and add each node:

```cpp
#include "ProductIndex.h"

ProductIndex builder;

// Traverse your product structure tree
for (each node in product_structure) {
    std::string productId = node.getProductId();  // e.g., "ID345345"
    int64_t nodeKey = node.getNodeKey();          // Vis Mockup node identifier
    
    builder.addRecord(productId, nodeKey);
}

// Sort and write to disk (this frees the memory)
builder.buildIndex("product_index.dat");
```

### Looking Up Nodes

When a user wants to select a part:

```cpp
ProductIndex index;
index.openIndex("product_index.dat");

int64_t nodeKey;
if (index.lookup("ID345345", nodeKey)) {
    // Found - tell Vis Mockup to select this node
    visMockup->SetNodeSelected(nodeKey, true);
} else {
    // Product ID not found in assembly
}
```

## API Reference

### ProductIndex Class

#### Building Methods

| Method | Description |
|--------|-------------|
| `void addRecord(const std::string& productId, int64_t nodeKey)` | Add a product ID to node key mapping (call during document load) |
| `bool buildIndex(const std::string& indexFilePath)` | Sort records and write to disk. Returns false on failure. Frees memory after writing. |

#### Search Methods

| Method | Description |
|--------|-------------|
| `bool openIndex(const std::string& indexFilePath)` | Open an existing index file for searching |
| `void closeIndex()` | Close the index file |
| `bool lookup(const std::string& productId, int64_t& outNodeKey)` | Find a node key by product ID. Returns true if found. |
| `size_t getRecordCount() const` | Get the number of records in the index |

## Configuration

Adjust `MAX_PRODUCT_ID_LEN` in `ProductIndex.h` based on your maximum product ID length:

```cpp
constexpr size_t MAX_PRODUCT_ID_LEN = 64;  // Adjust as needed
```

Shorter values reduce file size; longer values accommodate longer IDs. The value should be at least 1 byte longer than your longest product ID to accommodate null termination.

## File Format

The index file has a simple binary format:

```
[size_t: record_count]
[ProductRecord: record_0]
[ProductRecord: record_1]
...
[ProductRecord: record_n-1]
```

Each `ProductRecord` is a packed struct:
```cpp
struct ProductRecord {
    char productId[MAX_PRODUCT_ID_LEN];  // Null-padded string
    int64_t nodeKey;                      // 8 bytes
};
```

Records are sorted by `productId` in ascending lexicographic order.

## Building

### Requirements

- C++17 or later
- No external dependencies

### Compilation

```bash
# As part of your project
g++ -std=c++17 -O2 -c ProductIndex.cpp -o ProductIndex.o

# Link with your application
g++ -std=c++17 -O2 -o your_app your_app.cpp ProductIndex.o
```

### Running the Examples

```bash
# Basic example
g++ -std=c++17 -O2 -o example main.cpp ProductIndex.cpp
./example

# Performance test (2 million records)
g++ -std=c++17 -O2 -o perf_test perf_test.cpp ProductIndex.cpp
./perf_test
```

## Integration with COM DLL

This library is designed to be statically linked into your COM automation DLL, keeping deployment simple (single DLL to distribute).

Typical integration pattern:

```cpp
// In your COM class
class CVisMockupAutomation : public IVisMockupAutomation {
private:
    ProductIndex m_index;
    
public:
    HRESULT LoadDocument(BSTR path) {
        // Load document in Vis Mockup...
        
        // Build the index
        ProductIndex builder;
        TraverseProductStructure([&](const Node& node) {
            builder.addRecord(node.productId, node.nodeKey);
        });
        
        std::string indexPath = GetTempIndexPath();
        builder.buildIndex(indexPath);
        m_index.openIndex(indexPath);
        
        return S_OK;
    }
    
    HRESULT SelectByProductId(BSTR productId) {
        int64_t nodeKey;
        std::string id = BSTRToString(productId);
        
        if (m_index.lookup(id, nodeKey)) {
            // Fast path - found in index
            return m_pVisMockup->SetNodeSelected(nodeKey, TRUE);
        }
        return E_FAIL;
    }
};
```

## How It Works

1. **Build phase**: Records are collected in memory, sorted by product ID, then written sequentially to disk. Memory is freed after writing.

2. **Search phase**: Binary search is performed directly on the file using `seekg()` to jump to the middle record, read it, compare, and narrow the search range. Each lookup requires at most log₂(n) disk reads.

3. **OS caching**: After initial lookups, frequently accessed portions of the file are cached by the operating system, further improving performance.

## Limitations

- Product IDs are limited to `MAX_PRODUCT_ID_LEN - 1` characters
- The index is read-only after building (no incremental updates)
- Product IDs must be unique; duplicate IDs will return one of the matching records

## License

MIT License - feel free to use in commercial and open-source projects.
