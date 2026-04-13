#ifndef INDEX_STORE_H
#define INDEX_STORE_H

#include <string>
#include <string_view>
#include <mutex>
#include <map>
#include <unordered_map>
#include "InvertedIndexMap.hpp"
#include "StringList.hpp"

class IndexStore {
    StringList documents;
    
    InvertedIndexMap termInvertedIndex;

    mutable std::mutex documentMapMutex;

    public:
        IndexStore();
        virtual ~IndexStore() = default;

        IndexStore(const IndexStore&) = delete;
        IndexStore& operator=(const IndexStore&) = delete;
        IndexStore(IndexStore&&) = delete;
        IndexStore& operator=(IndexStore&&) = delete;

        long putDocument(std::string documentPath);
        std::string getDocument(long documentNumber);
        void updateIndex(long documentNumber, StringList &words);
        void updateIndex(long documentNumber, const std::unordered_map<std::string, long> &wordCounts);
        void addTerm(long documentNumber, const std::string &term);
        DocFreqList lookupIndex(std::string term);
};

#endif
