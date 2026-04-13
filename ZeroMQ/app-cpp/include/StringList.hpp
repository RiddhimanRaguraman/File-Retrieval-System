#ifndef STRING_LIST_H
#define STRING_LIST_H

#include <string>

class StringList {
    std::string* data;
    int capacity;
    int count;

    void resize();

public:
    StringList();
    ~StringList();

    // No copy/assign for simplicity unless needed
    StringList(const StringList& other);
    StringList& operator=(const StringList& other);

    void add(const std::string& val);
    int find(const std::string& val) const;
    std::string& get(int index) const;
    int size() const;
    void clear();
    std::string pop_back();
    
    // Iterator support for range-based for loops
    std::string* begin() const;
    std::string* end() const;
};

#endif
