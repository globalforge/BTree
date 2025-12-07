# BTreeLib

A high-performance, file-based indexing library for C++ applications. Provides two indexing strategies optimized for different use cases:

1. **BTree** - Generic B-tree with O(log n) disk reads per lookup
2. **Binary Search** - Sorted file with binary search, optimized for build-once/read-many workloads

Both implementations are designed for scenarios where data exceeds available RAM, such as indexing millions of CAD parts, product catalogs, or document metadata.

## Acknowledgments

The B-tree implementation in this library is based on the work of [Br. David Carlson](https://cis.stvincent.edu/html/carlson.html) from Saint Vincent College. His excellent [B-tree tutorial and implementation](https://cis.stvincent.edu/html/tutorials/swd/btree/btree.html) provided the foundation for this modernized C++17 version.

## Features

- **Header-only B-tree** - Single `BTree.h` file, easy integration
- **Configurable** - Key size, data size, and node size are template parameters
- **Type-safe API** - Template methods prevent buffer overflows at compile time
- **Disk-optimized** - Default 4KB node size aligns with filesystem blocks
- **MSVC & GCC compatible** - Builds cleanly on Windows and Linux
- **Modern C++17** - Uses RAII, move semantics, and standard library

## Quick Start

### B-Tree (Generic Key-Value Store)

```cpp
#include "BTree.h"

// 32-byte keys, 8-byte data, 4KB nodes
BTree<32, sizeof(int64_t)> tree;

// Write
tree.open("index.dat", BTreeMode::Write);
tree.insertValue("product-001", int64_t(12345));
tree.insertValue("product-002", int64_t(67890));
tree.close();

// Read
tree.open("index.dat", BTreeMode::Read);
int64_t value;
if (tree.retrieveValue("product-001", value)) {
    std::cout << "Found: " << value << std::endl;
}
```

### Binary Search (Optimized for Batch Loading)

```cpp
#include "ProductIndex.h"

ProductIndex index;

// Build phase - collect all records in memory
index.addRecord("ID001", 1001);
index.addRecord("ID002", 1002);
// ... add millions more ...

// Sort, write to disk, and auto-open for reading
index.buildIndex("products.dat");

// Lookup phase - binary search on disk
int64_t nodeKey;
if (index.lookup("ID001", nodeKey)) {
    std::cout << "Found node: " << nodeKey << std::endl;
}
```

## Performance Comparison

Benchmarks on Linux with SSD storage, compiled with `-O2`:

### Build Time

| Records | BTree | Binary Search | Winner |
|--------:|------:|--------------:|--------|
| 10,000 | 583 ms | 4 ms | **Binary Search (146x faster)** |
| 100,000 | 5,313 ms | 30 ms | **Binary Search (177x faster)** |
| 1,000,000 | 63,957 ms | 356 ms | **Binary Search (180x faster)** |
| 2,000,000 | 136,255 ms | 708 ms | **Binary Search (192x faster)** |

### Lookup Time (10,000 random lookups)

| Records | BTree | Binary Search | Winner |
|--------:|------:|--------------:|--------|
| 10,000 | 39 µs/lookup | 137 µs/lookup | **BTree (3.5x faster)** |
| 100,000 | 31 µs/lookup | 151 µs/lookup | **BTree (4.9x faster)** |
| 1,000,000 | 57 µs/lookup | 237 µs/lookup | **BTree (4.2x faster)** |
| 2,000,000 | 45 µs/lookup | 200 µs/lookup | **BTree (4.5x faster)** |

### Binary Search Build Time Breakdown

| Records | Add Records | Sort + Write | Total |
|--------:|------------:|-------------:|------:|
| 10,000 | 2 ms | 2 ms | 4 ms |
| 100,000 | 16 ms | 14 ms | 30 ms |
| 1,000,000 | 169 ms | 187 ms | 356 ms |
| 2,000,000 | 318 ms | 390 ms | 708 ms |

### Analysis

**Why is BTree build so slow?**
- Each insert requires disk I/O (read parent nodes, write modified nodes)
- Node splits cause additional writes
- Sequential inserts are worst-case for B-tree (causes maximum splits)

**Why is BTree lookup faster?**
- Order-86 B-tree: max 3-4 disk reads for 2M records (log₈₆ n)
- Binary search: max 21 disk reads for 2M records (log₂ n)
- B-tree nodes are read sequentially; binary search jumps around the file

**When to use each:**

| Scenario | Recommendation |
|----------|----------------|
| Build once, query many times | **Binary Search** |
| Frequent inserts during operation | **BTree** |
| Memory-constrained during build | **BTree** |
| Fastest possible build time | **Binary Search** |
| Fastest possible lookups | **BTree** |
| Simple deployment (one .h file) | **BTree** |

## API Reference

### BTree<KeySize, DataSize, NodeSize>

Template parameters:
- `KeySize` - Maximum key length in bytes (default: none, must specify)
- `DataSize` - Data payload size in bytes (default: none, must specify)
- `NodeSize` - Disk node size in bytes (default: 4096)

| Method | Description |
|--------|-------------|
| `open(filename, mode)` | Open for `BTreeMode::Read` or `BTreeMode::Write` |
| `close()` | Close the file (auto-called by destructor) |
| `insertValue<T>(key, value)` | Type-safe insert |
| `retrieveValue<T>(key, value)` | Type-safe lookup, returns `true` if found |
| `contains(key)` | Check existence |
| `size()` | Number of records |
| `empty()` | True if no records |

### Binary Search (ProductIndex class)

| Method | Description |
|--------|-------------|
| `addRecord(productId, nodeKey)` | Add record to in-memory buffer |
| `buildIndex(filename)` | Sort, write to disk, auto-open for reading |
| `openIndex(filename)` | Open existing index for reading |
| `closeIndex()` | Close the index file |
| `lookup(productId, nodeKey)` | Binary search, returns `true` if found |
| `containsRecord(productId)` | Check existence |
| `getRecordCount()` | Number of records |

## Building

### Requirements

- C++17 compiler (MSVC 2017+, GCC 7+, Clang 5+)
- No external dependencies

### Visual Studio 2022

1. Open `BTreeSolution.sln`
2. Build solution (F7)
3. Run `BTreeTest` or `ProductIndexTest` to verify

### GCC/Clang

```bash
# Build and run B-tree tests
g++ -std=c++17 -O2 -o btree_test BTreeTest.cpp
./btree_test

# Build with Binary Search
g++ -std=c++17 -O2 -o myapp myapp.cpp ProductIndex.cpp
```

### Integration

**Option 1: Header-only (BTree only)**
```cpp
#include "BTree.h"
// That's it!
```

**Option 2: Static library (includes Binary Search)**
- Add `ProductIndex.cpp` to your project
- Include headers as needed

## Project Structure

```
├── BTreeLib/
│   ├── BTree.h           # Header-only B-tree implementation
│   ├── ProductIndex.h    # Binary search index header
│   └── ProductIndex.cpp  # Binary search index implementation
├── BTreeTest/
│   └── BTreeTest.cpp     # B-tree unit tests (18 tests)
├── ProductIndexTest/
│   └── ProductIndexTest.cpp  # Binary search unit tests (5 tests)
└── BTreeSolution.sln     # Visual Studio 2022 solution
```

## Configuration

### BTree Node Size

The default 4KB node size works well for most systems. To optimize for your storage:

```cpp
// Check your filesystem block size (Windows PowerShell):
// fsutil fsinfo ntfsinfo C:
// Look for "Bytes Per Cluster"

// Match node size to cluster size
using MyTree = BTree<32, 8, 4096>;  // 4KB nodes (default)
using MyTree = BTree<32, 8, 8192>;  // 8KB nodes for larger clusters
```

### Binary Search Key Length

Adjust `MAX_PRODUCT_ID_LEN` in `ProductIndex.h`:

```cpp
constexpr size_t MAX_PRODUCT_ID_LEN = 64;  // Default: 64 bytes
```

## Limitations

- **No deletion** - Both implementations are insert-only (use tombstones or rebuild for deletions)
- **No range queries** - Point lookups only
- **No concurrent access** - External synchronization required for multi-threaded use
- **Unique keys only** - Duplicate keys throw (BTree) or return arbitrary match (Binary Search)

## Use Cases

- **CAD/PLM systems** - Index millions of parts by product ID
- **Document management** - Map document IDs to storage locations
- **Log indexing** - Index log entries by timestamp or event ID
- **Cache metadata** - Track cached items on disk
- **Game asset databases** - Index assets by name or hash

## License

MIT License - Free for commercial and open-source use.

## Contributing

Contributions welcome! Please include unit tests for new features.
