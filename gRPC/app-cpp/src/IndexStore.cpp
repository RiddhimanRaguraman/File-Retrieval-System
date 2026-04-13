#include "IndexStore.hpp"
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <vector>

IndexStore::IndexStore() : termInvertedIndex(3000017) { }

long IndexStore::putDocument(std::string documentPath) {
    std::lock_guard<std::mutex> lock(documentMapMutex);
    documents.add(documentPath);
    return documents.size();
}

std::string IndexStore::getDocument(long documentNumber) {
    std::lock_guard<std::mutex> lock(documentMapMutex);
    if (documentNumber < 1 || documentNumber > documents.size()) {
        return "";
    }
   
    return documents.get(documentNumber - 1);
}

void IndexStore::updateIndex(long documentNumber, StringList &words) {
    std::unordered_map<std::string, long> counts;
    for (const auto& word : words) {
        counts[word]++;
    }
    updateIndex(documentNumber, counts);
}

void IndexStore::updateIndex(long documentNumber, const std::unordered_map<std::string, long> &wordCounts) {
    // Parallelize the index update with 8 threads
    if (wordCounts.empty()) return;
    
    std::vector<std::pair<std::string, long>> items(wordCounts.begin(), wordCounts.end());
    size_t total = items.size();
    
    // If small number of items, just do it in current thread
    if (total < 1000) {
        for (const auto& pair : items) {
            termInvertedIndex.add(pair.first, documentNumber, pair.second);
        }
        return;
    }

    int numThreads = 8;
    size_t chunk = (total + numThreads - 1) / numThreads;
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &items, documentNumber, t, chunk, total]() {
            size_t start = t * chunk;
            size_t end = std::min(start + chunk, total);
            for (size_t i = start; i < end; ++i) {
                termInvertedIndex.add(items[i].first, documentNumber, items[i].second);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

void IndexStore::addTerm(long documentNumber, const std::string &term) {
    termInvertedIndex.add(term, documentNumber, 1);
}

DocFreqList IndexStore::lookupIndex(std::string term) {
    return termInvertedIndex.lookup(term);
}
