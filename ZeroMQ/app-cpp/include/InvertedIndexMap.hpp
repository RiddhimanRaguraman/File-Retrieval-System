#ifndef INVERTED_INDEX_MAP_H
#define INVERTED_INDEX_MAP_H

#include <vector>
#include <string>
#include <mutex>
#include "DocFreqList.hpp"

class InvertedIndexMap {
    struct Node {
        std::string key;
        DocFreqList value;
        Node* next;
    };

    Node** buckets;
    long bucketCount;
    
    // Fine-grained locking: one mutex for a group of buckets
    static const int NUM_LOCKS = 1024;
    std::mutex bucketMutexes[NUM_LOCKS];

    long hash(const std::string& key) const;

    public:
        InvertedIndexMap(long bucketCount);
        virtual ~InvertedIndexMap();

        InvertedIndexMap(const InvertedIndexMap&) = delete;
        InvertedIndexMap& operator=(const InvertedIndexMap&) = delete;
        InvertedIndexMap(InvertedIndexMap&&) = delete;
        InvertedIndexMap& operator=(InvertedIndexMap&&) = delete;

        // Thread-safe operations
        void add(const std::string& key, long docId, long amount);
        DocFreqList lookup(const std::string& key);

        // Deprecated/Unsafe if used concurrently without external lock
        bool contains(const std::string& key);
        DocFreqList& get(const std::string& key);
};

#endif
