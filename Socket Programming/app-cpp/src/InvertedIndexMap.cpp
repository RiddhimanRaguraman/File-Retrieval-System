#include "InvertedIndexMap.hpp"
#include <mutex>

InvertedIndexMap::InvertedIndexMap(long bucketCount) : bucketCount(bucketCount) {
    this->buckets = new Node*[bucketCount];
    for (long i = 0; i < bucketCount; i++) {
        this->buckets[i] = nullptr;
    }
}

InvertedIndexMap::~InvertedIndexMap() {
    for (long i = 0; i < bucketCount; i++) {
        Node* curr = buckets[i];
        while (curr) {
            Node* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }
    delete[] buckets;
}

long InvertedIndexMap::hash(const std::string& key) const {
    unsigned long hash = 5381;
    for (char c : key) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash % bucketCount;
}

void InvertedIndexMap::add(const std::string& key, long docId, long amount) {
    long idx = hash(key);
    int lockIdx = idx % NUM_LOCKS;
    
    std::lock_guard<std::mutex> lock(bucketMutexes[lockIdx]);
    
    Node* curr = buckets[idx];
    while (curr) {
        if (curr->key == key) {
            curr->value.addUnchecked(docId, amount);
            return;
        }
        curr = curr->next;
    }
    
    // Insert new
    Node* newNode = new Node;
    newNode->key = key;
    newNode->value.addUnchecked(docId, amount); 
    newNode->next = buckets[idx];
    buckets[idx] = newNode;
}

DocFreqList InvertedIndexMap::lookup(const std::string& key) {
    long idx = hash(key);
    int lockIdx = idx % NUM_LOCKS;
    
    std::lock_guard<std::mutex> lock(bucketMutexes[lockIdx]);
    
    Node* curr = buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return curr->value; // Return copy
        }
        curr = curr->next;
    }
    return DocFreqList(); // Return empty list
}

// Deprecated/Unsafe methods below (kept for compilation if needed, but should be avoided)

bool InvertedIndexMap::contains(const std::string& key) {
    long idx = hash(key);
    int lockIdx = idx % NUM_LOCKS;
    std::lock_guard<std::mutex> lock(bucketMutexes[lockIdx]);

    Node* curr = buckets[idx];
    while (curr) {
        if (curr->key == key) return true;
        curr = curr->next;
    }
    return false;
}

DocFreqList& InvertedIndexMap::get(const std::string& key) {
    // WARNING: This returns a reference to internal data without holding the lock!
    // Caller MUST ensure thread safety or this method is unsafe.
    // We lock during search/insertion, but the returned reference is unprotected.
    
    long idx = hash(key);
    int lockIdx = idx % NUM_LOCKS;
    std::lock_guard<std::mutex> lock(bucketMutexes[lockIdx]);

    Node* curr = buckets[idx];
    while (curr) {
        if (curr->key == key) {
            return curr->value;
        }
        curr = curr->next;
    }
    
    // Insert new
    Node* newNode = new Node;
    newNode->key = key;
    newNode->next = buckets[idx];
    buckets[idx] = newNode;
    return newNode->value;
}
