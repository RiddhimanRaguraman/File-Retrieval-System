#include "ServerProcessingEngine.hpp"
#include "Protocol.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <zmq.h>

ServerProcessingEngine::ServerProcessingEngine(std::shared_ptr<IndexStore> store) : store(std::move(store)) {}

void ServerProcessingEngine::initialize(int serverPort, int numberOfWorkerThreads) {
    if (running.exchange(true)) {
        return;
    }

    port = serverPort;
    workerCount = numberOfWorkerThreads;

    zmqContext = zmq_ctx_new();
    if (!zmqContext) {
        running = false;
        return;
    }

    proxyThread = std::thread(&ServerProcessingEngine::runProxy, this);

    {
        std::unique_lock<std::mutex> lock(proxyReadyMutex);
        proxyReadyCv.wait(lock, [&]() { return proxyReady || !running.load(); });
    }

    if (!running.load()) {
        return;
    }

    workerThreads.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i) {
        workerThreads.emplace_back(&ServerProcessingEngine::runWorker, this);
    }
}

void ServerProcessingEngine::shutdown() {
    if (!running.exchange(false)) {
        return;
    }

    if (zmqContext) {
        void* controlClient = zmq_socket(zmqContext, ZMQ_PAIR);
        if (controlClient) {
            const int lingerMs = 0;
            zmq_setsockopt(controlClient, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));
            if (zmq_connect(controlClient, "inproc://proxy-control") == 0) {
                const char terminateCmd[] = "TERMINATE";
                zmq_send(controlClient, terminateCmd, sizeof(terminateCmd) - 1, 0);
            }
            zmq_close(controlClient);
        }

        zmq_ctx_shutdown(zmqContext);
    }

    if (proxyThread.joinable()) {
        proxyThread.join();
    }
    for (auto& t : workerThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    workerThreads.clear();

    if (zmqContext) {
        zmq_ctx_term(zmqContext);
        zmqContext = nullptr;
    }
}

void ServerProcessingEngine::runProxy() {
    void* frontend = zmq_socket(zmqContext, ZMQ_ROUTER);
    void* backend = zmq_socket(zmqContext, ZMQ_DEALER);
    void* control = zmq_socket(zmqContext, ZMQ_PAIR);

    const int lingerMs = 0;
    if (frontend) zmq_setsockopt(frontend, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));
    if (backend) zmq_setsockopt(backend, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));
    if (control) zmq_setsockopt(control, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));

    bool ok = (frontend && backend && control);
    if (ok) {
        const std::string endpoint = "tcp://*:" + std::to_string(port);
        ok = zmq_bind(frontend, endpoint.c_str()) == 0;
    }
    if (ok) {
        ok = zmq_bind(backend, "inproc://backend") == 0;
    }
    if (ok) {
        ok = zmq_bind(control, "inproc://proxy-control") == 0;
    }

    {
        std::lock_guard<std::mutex> lock(proxyReadyMutex);
        proxyReady = true;
    }
    proxyReadyCv.notify_all();

    if (ok) {
        zmq_proxy_steerable(frontend, backend, nullptr, control);
    } else {
        running = false;
    }

    if (control) zmq_close(control);
    if (backend) zmq_close(backend);
    if (frontend) zmq_close(frontend);
}

void ServerProcessingEngine::runWorker() {
    void* worker = zmq_socket(zmqContext, ZMQ_REP);
    if (!worker) {
        return;
    }
    const int lingerMs = 0;
    zmq_setsockopt(worker, ZMQ_LINGER, &lingerMs, sizeof(lingerMs));

    if (zmq_connect(worker, "inproc://backend") != 0) {
        zmq_close(worker);
        return;
    }

    while (running.load()) {
        std::vector<std::vector<std::uint8_t>> frames;
        if (!zmqRecvMultipart(worker, frames)) {
            if (zmq_errno() == ETERM) {
                break;
            }
            continue;
        }
        if (frames.empty()) {
            continue;
        }

        const auto& payload = frames.back();
        ByteReader reader(std::span<const std::uint8_t>(payload.data(), payload.size()));
        MessageType type;
        if (!readMessageType(reader, type)) {
            continue;
        }
        ByteWriter replyWriter;

        if (type == MessageType::REGISTER_REQUEST) {
            const std::uint32_t id = nextClientId.fetch_add(1);
            writeMessageType(replyWriter, MessageType::REGISTER_REPLY);
            replyWriter.writeU32(id);
        } else if (type == MessageType::INDEX_REQUEST) {
            std::uint32_t reqClientId = 0;
            std::string docPath;
            std::uint32_t numWords = 0;
            std::unordered_map<std::string, int> wordCounts;

            bool ok = reader.readU32(reqClientId) && reader.readString(docPath) && reader.readU32(numWords);
            if (ok) {
                for (std::uint32_t i = 0; i < numWords; ++i) {
                    std::string word;
                    std::uint32_t freq = 0;
                    if (!reader.readString(word) || !reader.readU32(freq)) {
                        ok = false;
                        break;
                    }
                    wordCounts[word] = static_cast<int>(freq);
                }
            }

            writeMessageType(replyWriter, MessageType::INDEX_REPLY);
            if (ok) {
                const std::string fullPath = "client" + std::to_string(reqClientId) + ":" + docPath;
                const long docNum = store->putDocument(fullPath);
                store->updateIndex(docNum, wordCounts);
                replyWriter.writeU32(0);
            } else {
                replyWriter.writeU32(1);
            }
        } else if (type == MessageType::SEARCH_REQUEST) {
            std::uint32_t numTerms = 0;
            bool ok = reader.readU32(numTerms) && numTerms > 0;

            std::vector<std::string> terms;
            if (ok) {
                terms.reserve(numTerms);
                for (std::uint32_t i = 0; i < numTerms; ++i) {
                    std::string term;
                    if (!reader.readString(term)) {
                        ok = false;
                        break;
                    }
                    terms.push_back(std::move(term));
                }
            }

            std::map<long, long> docScores;
            if (ok) {
                bool firstTerm = true;
                for (const auto& term : terms) {
                    DocFreqList list = store->lookupIndex(term);
                    std::map<long, long> currentTermDocs;
                    for (int i = 0; i < list.size(); ++i) {
                        currentTermDocs[list.getDocId(i)] = list.getFreq(i);
                    }
                    if (firstTerm) {
                        docScores = std::move(currentTermDocs);
                        firstTerm = false;
                    } else {
                        std::map<long, long> nextDocScores;
                        for (const auto& pair : docScores) {
                            const auto it = currentTermDocs.find(pair.first);
                            if (it != currentTermDocs.end()) {
                                nextDocScores[pair.first] = pair.second + it->second;
                            }
                        }
                        docScores = std::move(nextDocScores);
                    }
                    if (docScores.empty()) {
                        break;
                    }
                }
            }

            struct DocResult {
                long docId;
                long freq;
                std::string path;
            };
            std::vector<DocResult> sortedDocs;
            sortedDocs.reserve(docScores.size());
            for (const auto& pair : docScores) {
                sortedDocs.push_back({pair.first, pair.second, store->getDocument(pair.first)});
            }

            std::sort(sortedDocs.begin(), sortedDocs.end(), [](const DocResult& a, const DocResult& b) {
                if (a.freq != b.freq) {
                    return a.freq > b.freq;
                }
                return a.path < b.path;
            });

            const std::uint32_t totalHits = static_cast<std::uint32_t>(sortedDocs.size());
            if (sortedDocs.size() > 10) {
                sortedDocs.resize(10);
            }

            writeMessageType(replyWriter, MessageType::SEARCH_REPLY);
            replyWriter.writeU32(totalHits);
            replyWriter.writeU32(static_cast<std::uint32_t>(sortedDocs.size()));
            for (const auto& doc : sortedDocs) {
                replyWriter.writeString(doc.path);
                replyWriter.writeU32(static_cast<std::uint32_t>(doc.freq));
            }
        } else {
            writeMessageType(replyWriter, MessageType::QUIT);
            replyWriter.writeU32(1);
        }

        auto replyFrames = frames;
        replyFrames.back() = replyWriter.takeBytes();
        zmqSendMultipart(worker, replyFrames);
    }

    zmq_close(worker);
}

std::vector<std::string> ServerProcessingEngine::getConnectedClients() {
    return {};
}
