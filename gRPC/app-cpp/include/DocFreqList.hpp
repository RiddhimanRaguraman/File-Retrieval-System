#ifndef DOC_FREQ_LIST_H
#define DOC_FREQ_LIST_H

class DocFreqList {
    long* docIds; //DocFreqPair.documentNumber
    long* freqs;  //DocFreqPair.wordFrequency
    int capacity;
    int count;

    void resize();

public:
    DocFreqList();
    ~DocFreqList();

    DocFreqList(const DocFreqList& other);
    DocFreqList& operator=(const DocFreqList& other);

    void add(long docId, long freq);
    void addFrequency(long docId, long amount);
    void addUnchecked(long docId, long amount);
    
    long getDocId(int index) const;
    long getFreq(int index) const;
    int size() const;
};

#endif
