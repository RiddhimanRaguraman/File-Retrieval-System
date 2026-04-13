#ifndef RESULT_LIST_H
#define RESULT_LIST_H

#include <string>

struct DocPathFreqPair {
    std::string documentPath;
    long wordFrequency;
};

// Custom result list instead of vector
class ResultList {
    DocPathFreqPair* data;
    int capacity;
    int count;
    void resize();
public:
    ResultList();
    ~ResultList();
    ResultList(const ResultList& other);
    ResultList& operator=(const ResultList& other);
    void add(const DocPathFreqPair& val);
    DocPathFreqPair& get(int index) const;
    void truncate(int newSize);
    int size() const;
    DocPathFreqPair* begin() const;
    DocPathFreqPair* end() const;
};

#endif
