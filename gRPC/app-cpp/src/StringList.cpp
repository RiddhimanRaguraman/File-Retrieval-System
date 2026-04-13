#include "StringList.hpp"

StringList::StringList() {
    capacity = 10;
    count = 0;
    data = new std::string[capacity];
}

StringList::~StringList() {
    delete[] data;
}

StringList::StringList(const StringList& other) {
    capacity = other.capacity;
    count = other.count;
    data = new std::string[capacity];
    for (int i = 0; i < count; i++) {
        data[i] = other.data[i];
    }
}

StringList& StringList::operator=(const StringList& other) {
    if (this != &other) {
        delete[] data;
        capacity = other.capacity;
        count = other.count;
        data = new std::string[capacity];
        for (int i = 0; i < count; i++) {
            data[i] = other.data[i];
        }
    }
    return *this;
}

void StringList::resize() {
    int newCapacity = capacity * 2;
    std::string* newData = new std::string[newCapacity];
    for (int i = 0; i < count; i++) {
        newData[i] = data[i];
    }
    delete[] data;
    data = newData;
    capacity = newCapacity;
}

void StringList::add(const std::string& val) {
    if (count == capacity) {
        resize();
    }
    data[count++] = val;
}

int StringList::find(const std::string& val) const {
    for (int i = 0; i < count; i++) {
        if (data[i] == val) {
            return i;
        }
    }
    return -1;
}

std::string& StringList::get(int index) const {
    if (index < 0 || index >= count) {
        static std::string empty = "";
        return empty;
    }
    return data[index];
}

int StringList::size() const {
    return count;
}

void StringList::clear() {
    count = 0;
}

std::string StringList::pop_back() {
    if (count > 0) {
        count--;
        return data[count];
    }
    return "";
}

std::string* StringList::begin() const {
    return data;
}

std::string* StringList::end() const {
    return data + count;
}
