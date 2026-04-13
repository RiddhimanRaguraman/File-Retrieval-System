#include "ResultList.hpp"

ResultList::ResultList() {
    capacity = 10;
    count = 0;
    data = new DocPathFreqPair[capacity];
}

ResultList::~ResultList() {
    delete[] data;
}

ResultList::ResultList(const ResultList& other) {
    capacity = other.capacity;
    count = other.count;
    data = new DocPathFreqPair[capacity];
    for (int i = 0; i < count; i++) {
        data[i] = other.data[i];
    }
}

ResultList& ResultList::operator=(const ResultList& other) {
    if (this != &other) {
        delete[] data;
        capacity = other.capacity;
        count = other.count;
        data = new DocPathFreqPair[capacity];
        for (int i = 0; i < count; i++) {
            data[i] = other.data[i];
        }
    }
    return *this;
}

void ResultList::resize() {
    int newCapacity = capacity * 2;
    DocPathFreqPair* newData = new DocPathFreqPair[newCapacity];
    for (int i = 0; i < count; i++) {
        newData[i] = data[i];
    }
    delete[] data;
    data = newData;
    capacity = newCapacity;
}

void ResultList::add(const DocPathFreqPair& val) {
    if (count == capacity) {
        resize();
    }
    data[count++] = val;
}

DocPathFreqPair& ResultList::get(int index) const {
    if (index < 0 || index >= count) {
        static DocPathFreqPair empty = {"", 0};
        return empty;
    }
    return data[index];
}

void ResultList::truncate(int newSize) {
    if (newSize < count) {
        count = newSize;
    }
}

int ResultList::size() const {
    return count;
}

DocPathFreqPair* ResultList::begin() const {
    return data;
}

DocPathFreqPair* ResultList::end() const {
    return data + count;
}
