#pragma once
/*
 * BTree.h - A generic, file-based B-tree implementation
 * 
 * Features:
 *   - Configurable key and data sizes
 *   - Configurable node size (for disk block alignment)
 *   - Order automatically calculated from node size
 *   - Single file for both writing and reading
 *   - Simple API: Insert, Retrieve, Contains
 * 
 * Usage:
 *   // Create a new B-tree (write mode)
 *   BTree<32, 64> tree;  // 32-byte keys, 64-byte data
 *   tree.open("index.dat", BTreeMode::Write);
 *   tree.insert("mykey", myData);
 *   tree.close();
 * 
 *   // Read from existing B-tree
 *   tree.open("index.dat", BTreeMode::Read);
 *   if (tree.retrieve("mykey", myData)) { ... }
 */

// MSVC compatibility
#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
    #pragma warning(push)
    #pragma warning(disable: 4996)  // strncpy deprecation
#endif

#include <fstream>
#include <cstring>
#include <string>
#include <stdexcept>
#include <algorithm>

enum class BTreeMode {
    Read,
    Write
};

template <
    size_t KeySize,                    // Maximum key size in bytes
    size_t DataSize,                   // Data size in bytes
    size_t NodeSize = 4096             // Node size (align to disk block)
>
class BTree {
public:
    // Calculate B-tree order from node size
    // Node layout: [Count:4][Keys:Order-1][Branches:Order]
    // Where each Key = KeySize + DataSize, each Branch = 8 bytes (long)
    static constexpr size_t RecordSize = KeySize + DataSize;
    static constexpr size_t OrderCalc = (NodeSize - sizeof(int) + RecordSize + sizeof(long)) 
                                        / (RecordSize + sizeof(long));
    static constexpr size_t Order = (OrderCalc < 3) ? 3 : OrderCalc;  // Minimum order of 3
    static constexpr size_t MaxKeys = Order - 1;
    static constexpr size_t MinKeys = (Order - 1) / 2;
    static constexpr long NilPtr = -1L;

    // Public record type for external use
    struct Record {
        char key[KeySize];
        char data[DataSize];

        Record() {
            std::memset(key, 0, KeySize);
            std::memset(data, 0, DataSize);
        }

        Record(const char* k, const char* d) {
            std::memset(key, 0, KeySize);
            std::memset(data, 0, DataSize);
            if (k) std::strncpy(key, k, KeySize - 1);
            if (d) std::memcpy(data, d, DataSize);
        }

        bool operator<(const Record& other) const {
            return std::strcmp(key, other.key) < 0;
        }
    };

private:
    #pragma pack(push, 1)
    struct Node {
        int count;                      // Number of keys in this node
        Record records[MaxKeys];        // Key-data pairs
        long branches[Order];           // Child pointers (node numbers)

        Node() : count(0) {
            std::fill(std::begin(branches), std::end(branches), NilPtr);
        }
    };
    #pragma pack(pop)

    // File layout: [Node 0: metadata][Node 1: first real node][Node 2]...
    // Node 0 stores: branches[0]=numItems, branches[1]=numNodes, branches[2]=root

    std::fstream file_;
    BTreeMode mode_;
    bool isOpen_;

    long root_;
    long numItems_;
    long numNodes_;

    Node currentNode_;  // Working buffer

    static constexpr size_t ActualNodeSize = sizeof(Node);

public:
    BTree() : mode_(BTreeMode::Read), isOpen_(false), 
              root_(NilPtr), numItems_(0), numNodes_(0) {}

    ~BTree() {
        close();
    }

    // Non-copyable
    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;

    // Movable
    BTree(BTree&& other) noexcept {
        *this = std::move(other);
    }

    BTree& operator=(BTree&& other) noexcept {
        if (this != &other) {
            close();
            file_ = std::move(other.file_);
            mode_ = other.mode_;
            isOpen_ = other.isOpen_;
            root_ = other.root_;
            numItems_ = other.numItems_;
            numNodes_ = other.numNodes_;
            currentNode_ = other.currentNode_;
            other.isOpen_ = false;
        }
        return *this;
    }

    /**
     * Open a B-tree file for reading or writing.
     * In Write mode, creates a new empty tree (overwrites existing file).
     * In Read mode, opens an existing tree file.
     */
    bool open(const std::string& filename, BTreeMode mode) {
        close();
        mode_ = mode;

        if (mode == BTreeMode::Read) {
            file_.open(filename, std::ios::in | std::ios::binary);
            if (!file_) return false;

            // Read metadata from node 0
            if (!readNode(0, currentNode_)) {
                file_.close();
                return false;
            }
            numItems_ = currentNode_.branches[0];
            numNodes_ = currentNode_.branches[1];
            root_ = currentNode_.branches[2];
        }
        else {  // Write mode
            file_.open(filename, std::ios::in | std::ios::out | 
                       std::ios::trunc | std::ios::binary);
            if (!file_) return false;

            root_ = NilPtr;
            numItems_ = 0;
            numNodes_ = 0;

            // Write initial metadata to node 0
            writeMetadata();
        }

        isOpen_ = true;
        return true;
    }

    /**
     * Close the B-tree file.
     * In Write mode, ensures metadata is flushed.
     */
    void close() {
        if (!isOpen_) return;

        if (mode_ == BTreeMode::Write) {
            writeMetadata();
        }

        file_.close();
        isOpen_ = false;
        root_ = NilPtr;
        numItems_ = 0;
        numNodes_ = 0;
    }

    /**
     * Insert a key-data pair into the B-tree.
     * Throws if the tree is not open in Write mode.
     * Throws if a duplicate key is inserted.
     * 
     * Note: data must point to a buffer of at least DataSize bytes.
     * For type-safe insertion, use the templated insertValue() method.
     */
    void insert(const char* key, const void* data) {
        if (!isOpen_ || mode_ != BTreeMode::Write) {
            throw std::runtime_error("BTree not open for writing");
        }

        Record record;
        std::strncpy(record.key, key, KeySize - 1);
        std::memcpy(record.data, data, DataSize);

        bool moveUp = false;
        Record newRecord;
        long newRight = NilPtr;

        pushDown(record, root_, moveUp, newRecord, newRight);

        if (moveUp) {
            // Create new root
            Node newRoot;
            newRoot.count = 1;
            newRoot.records[0] = newRecord;
            newRoot.branches[0] = root_;
            newRoot.branches[1] = newRight;

            numNodes_++;
            root_ = numNodes_;
            writeNode(root_, newRoot);
        }

        numItems_++;
    }

    /**
     * Type-safe insert: stores a value of type T.
     * T must fit within DataSize bytes.
     */
    template<typename T>
    void insertValue(const char* key, const T& value) {
        static_assert(sizeof(T) <= DataSize, "Value type exceeds DataSize");
        char buffer[DataSize] = {0};
        std::memcpy(buffer, &value, sizeof(T));
        insert(key, buffer);
    }

    template<typename T>
    void insertValue(const std::string& key, const T& value) {
        insertValue(key.c_str(), value);
    }

    /**
     * Insert with string key (convenience overload)
     */
    void insert(const std::string& key, const void* data) {
        insert(key.c_str(), data);
    }

    /**
     * Retrieve data associated with a key.
     * Returns true if found, false otherwise.
     * 
     * Note: data must point to a buffer of at least DataSize bytes.
     * For type-safe retrieval, use the templated retrieveValue() method.
     */
    bool retrieve(const char* key, void* data) {
        if (!isOpen_) return false;

        char searchKey[KeySize];
        std::memset(searchKey, 0, KeySize);
        std::strncpy(searchKey, key, KeySize - 1);

        long current = root_;
        int location;

        while (current != NilPtr) {
            readNode(current, currentNode_);

            if (searchNode(searchKey, location)) {
                std::memcpy(data, currentNode_.records[location].data, DataSize);
                return true;
            }
            current = currentNode_.branches[location + 1];
        }

        return false;
    }

    /**
     * Type-safe retrieve: retrieves a value of type T.
     * T must fit within DataSize bytes.
     */
    template<typename T>
    bool retrieveValue(const char* key, T& value) {
        static_assert(sizeof(T) <= DataSize, "Value type exceeds DataSize");
        char buffer[DataSize];
        if (retrieve(key, buffer)) {
            std::memcpy(&value, buffer, sizeof(T));
            return true;
        }
        return false;
    }

    template<typename T>
    bool retrieveValue(const std::string& key, T& value) {
        return retrieveValue(key.c_str(), value);
    }

    /**
     * Retrieve with string key (convenience overload)
     */
    bool retrieve(const std::string& key, void* data) {
        return retrieve(key.c_str(), data);
    }

    /**
     * Check if a key exists in the B-tree.
     */
    bool contains(const char* key) {
        char unused[DataSize];
        return retrieve(key, unused);
    }

    bool contains(const std::string& key) {
        return contains(key.c_str());
    }

    /**
     * Returns true if the tree is empty.
     */
    bool empty() const {
        return root_ == NilPtr;
    }

    /**
     * Returns the number of items in the tree.
     */
    long size() const {
        return numItems_;
    }

    /**
     * Returns true if the tree is open.
     */
    bool isOpen() const {
        return isOpen_;
    }

    /**
     * Get configuration info (useful for debugging/verification)
     */
    static constexpr size_t keySize() { return KeySize; }
    static constexpr size_t dataSize() { return DataSize; }
    static constexpr size_t nodeSize() { return ActualNodeSize; }
    static constexpr size_t order() { return Order; }
    static constexpr size_t maxKeys() { return MaxKeys; }
    static constexpr size_t minKeys() { return MinKeys; }

private:
    void writeMetadata() {
        Node metaNode;
        metaNode.branches[0] = numItems_;
        metaNode.branches[1] = numNodes_;
        metaNode.branches[2] = root_;
        writeNode(0, metaNode);
    }

    bool readNode(long nodeNum, Node& node) {
        file_.seekg(nodeNum * ActualNodeSize, std::ios::beg);
        file_.read(reinterpret_cast<char*>(&node), ActualNodeSize);
        return !file_.fail();
    }

    bool writeNode(long nodeNum, const Node& node) {
        file_.seekp(nodeNum * ActualNodeSize, std::ios::beg);
        file_.write(reinterpret_cast<const char*>(&node), ActualNodeSize);
        return !file_.fail();
    }

    /**
     * Search for a key within the current node.
     * Returns true if found, with location set to the index.
     * If not found, location is set such that the key would be
     * between location and location+1.
     */
    bool searchNode(const char* target, int& location) {
        if (currentNode_.count == 0) {
            location = -1;
            return false;
        }

        if (std::strcmp(target, currentNode_.records[0].key) < 0) {
            location = -1;
            return false;
        }

        // Sequential search from right to left
        location = currentNode_.count - 1;
        while (location > 0 && 
               std::strcmp(target, currentNode_.records[location].key) < 0) {
            location--;
        }

        return std::strcmp(target, currentNode_.records[location].key) == 0;
    }

    /**
     * Add a record to a node at the given location,
     * shifting existing records to make room.
     */
    void addRecord(const Record& record, long rightBranch, 
                   Node& node, int location) {
        for (int j = node.count; j > location; j--) {
            node.records[j] = node.records[j - 1];
            node.branches[j + 1] = node.branches[j];
        }
        node.records[location] = record;
        node.branches[location + 1] = rightBranch;
        node.count++;
    }

    /**
     * Split a full node into two nodes.
     */
    void split(const Record& currentRecord, long currentRight,
               long currentRoot, int location,
               Record& newRecord, long& newRight) {
        
        int median = (location < static_cast<int>(MinKeys)) 
                     ? MinKeys : MinKeys + 1;

        readNode(currentRoot, currentNode_);

        Node rightNode;
        
        // Move upper half to right node
        for (int j = median; j < static_cast<int>(MaxKeys); j++) {
            rightNode.records[j - median] = currentNode_.records[j];
            rightNode.branches[j - median + 1] = currentNode_.branches[j + 1];
        }

        rightNode.count = MaxKeys - median;
        currentNode_.count = median;

        // Insert the current record into appropriate node
        if (location < static_cast<int>(MinKeys)) {
            addRecord(currentRecord, currentRight, currentNode_, location + 1);
        } else {
            addRecord(currentRecord, currentRight, rightNode, 
                      location - median + 1);
        }

        // The median record moves up
        newRecord = currentNode_.records[currentNode_.count - 1];
        rightNode.branches[0] = currentNode_.branches[currentNode_.count];
        currentNode_.count--;

        writeNode(currentRoot, currentNode_);

        numNodes_++;
        newRight = numNodes_;
        writeNode(newRight, rightNode);
    }

    /**
     * Recursive insert helper.
     */
    void pushDown(const Record& currentRecord, long currentRoot,
                  bool& moveUp, Record& newRecord, long& newRight) {
        
        if (currentRoot == NilPtr) {
            // Reached a leaf position
            moveUp = true;
            newRecord = currentRecord;
            newRight = NilPtr;
            return;
        }

        readNode(currentRoot, currentNode_);

        int location;
        if (searchNode(currentRecord.key, location)) {
            throw std::runtime_error("Duplicate key insertion attempted");
        }

        pushDown(currentRecord, currentNode_.branches[location + 1],
                 moveUp, newRecord, newRight);

        if (moveUp) {
            readNode(currentRoot, currentNode_);

            if (currentNode_.count < static_cast<int>(MaxKeys)) {
                moveUp = false;
                addRecord(newRecord, newRight, currentNode_, location + 1);
                writeNode(currentRoot, currentNode_);
            } else {
                split(newRecord, newRight, currentRoot, location,
                      newRecord, newRight);
            }
        }
    }
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
