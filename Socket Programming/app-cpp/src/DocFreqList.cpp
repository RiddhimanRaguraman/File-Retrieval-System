#include "DocFreqList.hpp"

DocFreqList::DocFreqList() {
    capacity = 10;
    count = 0;
    docIds = new long[capacity];
    freqs = new long[capacity];
}

DocFreqList::~DocFreqList() {
    delete[] docIds;
    delete[] freqs;
}

DocFreqList::DocFreqList(const DocFreqList& other) {
    capacity = other.capacity;
    count = other.count;
    docIds = new long[capacity];
    freqs = new long[capacity];
    for (int i = 0; i < count; i++) {
        docIds[i] = other.docIds[i];
        freqs[i] = other.freqs[i];
    }
}

DocFreqList& DocFreqList::operator=(const DocFreqList& other) {
    if (this != &other) {
        delete[] docIds;
        delete[] freqs;
        
        capacity = other.capacity;
        count = other.count;
        docIds = new long[capacity];
        freqs = new long[capacity];
        for (int i = 0; i < count; i++) {
            docIds[i] = other.docIds[i];
            freqs[i] = other.freqs[i];
        }
    }
    return *this;
}

void DocFreqList::resize() {
    int newCapacity = capacity * 2;
    long* newDocIds = new long[newCapacity];
    long* newFreqs = new long[newCapacity];
    
    for (int i = 0; i < count; i++) {
        newDocIds[i] = docIds[i];
        newFreqs[i] = freqs[i];
    }
    
    delete[] docIds;
    delete[] freqs;
    
    docIds = newDocIds;
    freqs = newFreqs;
    capacity = newCapacity;
}

void DocFreqList::add(long docId, long freq) {
    if (count == capacity) {
        resize();
    }
    docIds[count] = docId;
    freqs[count] = freq;
    count++;
}

void DocFreqList::addFrequency(long docId, long amount) {
    // Optimization: Check the last element first, as we are likely appending to the same document
    if (count > 0 && docIds[count - 1] == docId) {
        freqs[count - 1] += amount;
        return;
    }

    for (int i = 0; i < count - 1; i++) {
        if (docIds[i] == docId) {
            freqs[i] += amount;
            return;
        }
    }
    // If not found, add new
    add(docId, amount);
}

void DocFreqList::addUnchecked(long docId, long amount) {
    // Optimization: Check the last element first, as we are likely appending to the same document
    if (count > 0 && docIds[count - 1] == docId) {
        freqs[count - 1] += amount;
        return;
    }
    // Directly append without scanning the whole list
    add(docId, amount);
}

long DocFreqList::getDocId(int index) const {
    return docIds[index];
}

long DocFreqList::getFreq(int index) const {
    return freqs[index];
}

int DocFreqList::size() const {
    return count;
}
