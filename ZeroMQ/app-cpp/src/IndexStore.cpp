#include "IndexStore.hpp"
#include <unordered_map>

IndexStore::IndexStore() : termInvertedIndex(3000017) { }

long IndexStore::putDocument(std::string documentPath) {
    std::lock_guard<std::mutex> lock(documentMapMutex);
    const auto it = documentPathToId.find(documentPath);
    if (it != documentPathToId.end()) {
        return it->second;
    }

    documents.add(documentPath);
    const long id = static_cast<long>(documents.size());
    documentPathToId.emplace(std::move(documentPath), id);
    return id;
}

std::string IndexStore::getDocument(long documentNumber) {
    std::lock_guard<std::mutex> lock(documentMapMutex);
    if (documentNumber < 1 || documentNumber > documents.size()) {
        return "";
    }
    return documents.get(documentNumber - 1);
}

void IndexStore::updateIndex(long documentNumber, StringList &words) {
    // Deprecated method used by old code
    // Convert to map and call the optimized version
    std::unordered_map<std::string, int> wordCounts;
    for (const auto& word : words) {
        wordCounts[word]++;
    }
    updateIndex(documentNumber, wordCounts);
}

void IndexStore::updateIndex(long documentNumber, const std::unordered_map<std::string, int> &wordCounts) {
    for (const auto& pair : wordCounts) {
        termInvertedIndex.add(pair.first, documentNumber, pair.second);
    }
}

void IndexStore::addTerm(long documentNumber, const std::string &term) {
    termInvertedIndex.add(term, documentNumber, 1);
}

DocFreqList IndexStore::lookupIndex(std::string term) {
    return termInvertedIndex.lookup(term);
}
