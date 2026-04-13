# File Retrieval Engine — Networking Stack Comparison

A distributed file retrieval engine implemented three times, each using a different networking stack: **Raw Socket Programming**, **ZeroMQ**, and **gRPC**. The goal is to index large collections of documents across multiple parallel clients and search them efficiently, while comparing how each networking approach affects performance and scalability.

---

## Table of Contents

- [Project Overview](#project-overview)
- [System Architecture](#system-architecture)
- [Networking Implementations](#networking-implementations)
  - [Socket Programming](#1-socket-programming)
  - [ZeroMQ](#2-zeromq)
  - [gRPC](#3-grpc)
- [Protocol & Message Design](#protocol--message-design)
- [Performance Results](#performance-results)
  - [Dataset 1 — 134 MB](#dataset-1--134-mb)
  - [Dataset 2 — 537 MB](#dataset-2--537-mb)
  - [Dataset 3 — 2145 MB](#dataset-3--2145-mb)
- [Head-to-Head Comparison](#head-to-head-comparison)
- [Key Design Decisions](#key-design-decisions)
- [Building & Running](#building--running)

---

## Project Overview

This project builds a **distributed inverted-index file retrieval system** in C++. Clients scan a directory of documents, compute word frequencies locally, and push the index data to a central server. The server aggregates everything into a shared inverted index, which any client can then query with a multi-term search.

The same application logic is implemented with three different transport layers to measure real-world throughput, scalability, and ease of implementation across each approach.

**Datasets used:**

| Dataset | Size |
|---------|------|
| Dataset 1 | 134.25 MB |
| Dataset 2 | 537.20 MB |
| Dataset 3 | 2,145.18 MB |

---

## System Architecture

```
┌─────────────┐     INDEX_REQUEST      ┌──────────────────────┐
│  Client 1   │ ──────────────────────► │                      │
├─────────────┤                         │   Server             │
│  Client 2   │ ──────────────────────► │   (Inverted Index)   │
├─────────────┤     SEARCH_REQUEST      │                      │
│  Client 3   │ ◄──────────────────────►│   Thread-per-client  │
├─────────────┤                         │   + Stripe Locking   │
│  Client 4   │ ──────────────────────► │                      │
└─────────────┘                         └──────────────────────┘
```

**Client responsibilities:**
- Walk a directory of files
- Read each file, tokenize it, and count word frequencies (client-side computation)
- Send `{term → frequency}` pairs to the server via `INDEX_REQUEST`
- Issue multi-term `SEARCH_REQUEST` and receive ranked results

**Server responsibilities:**
- Register clients and assign unique IDs
- Merge incoming index data into a shared inverted index
- Handle concurrent clients using per-client threads and stripe-locked mutexes (1024 stripes) to minimize contention
- Serve search results ranked by combined word frequency

---

## Networking Implementations

### 1. Socket Programming

**Directory:** `Socket Programming/`

The baseline implementation uses raw POSIX TCP sockets with a fully hand-rolled binary protocol.

- The server runs a `dispatcherThread` that accepts connections and spawns a dedicated `std::thread` per client (`runWorker`), keeping the TCP connection alive for the entire session.
- Messages are serialized with a custom `ByteWriter`/`ByteReader` using `uint32` fields and length-prefixed strings, sent in network byte order.
- All synchronization is manual — the developer manages every lock, thread, and buffer.

**Thread strategy:** On-demand thread creation (one thread per connected client).

> **Advantage:** Simple to implement; the persistent connection avoids per-request TCP handshake overhead.  
> **Disadvantage:** Does not scale to very large numbers of concurrent clients since each consumes a dedicated OS thread and stack memory.

---

### 2. ZeroMQ

**Directory:** `ZeroMQ/`

ZeroMQ replaces raw sockets with its high-level async messaging library, using the DEALER/ROUTER pattern. The same custom binary protocol (`ByteWriter`/`ByteReader`) is used over ZeroMQ multipart messages.

- The server uses ZeroMQ's built-in worker thread pool to handle concurrent clients.
- Message routing is handled by ZeroMQ's identity frames rather than by manually accepting connections.
- The developer still controls serialization but delegates connection management and I/O multiplexing to ZeroMQ.

**Message framing:** Each multipart ZeroMQ message carries an identity frame (for routing) plus a payload frame encoded with the same binary protocol as the socket version.

---

### 3. gRPC

**Directory:** (gRPC implementation)

gRPC replaces both the transport layer and serialization with Protocol Buffers (proto3) over HTTP/2. RPC definitions are declared in a `.proto` file and stubs are generated automatically.

- The server uses gRPC's built-in async thread pool; no manual thread management is required.
- Messages are strongly typed, versioned, and self-describing via proto3 schemas.
- HTTP/2 multiplexing allows multiple in-flight requests over a single connection.

**RPC methods defined:**

| Method | Request | Response |
|--------|---------|----------|
| `Register` | *(empty)* | `RegisterRep { client_id }` |
| `ComputeIndex` | `IndexReq { client_id, document_path, word_frequencies }` | `IndexRep { status }` |
| `ComputeSearch` | `SearchReq { terms[] }` | `SearchRep { path → score }` |

---

## Protocol & Message Design

All three implementations share the same logical message types. The encoding differs:

| Message | Socket / ZeroMQ (Binary) | gRPC (proto3) |
|---------|--------------------------|---------------|
| **Register Request** | `MessageType::REGISTER_REQUEST` | Empty proto message |
| **Register Reply** | `REGISTER_REPLY` + `clientId (u32)` | `RegisterRep { client_id }` |
| **Index Request** | `INDEX_REQUEST` + `clientId` + `filePath` + `termCount` + `[(term, freq)...]` | `IndexReq { client_id, document_path, word_frequencies map }` |
| **Index Reply** | `INDEX_REPLY` + `status (u32)` | `IndexRep { acknowledgment }` |
| **Search Request** | `SEARCH_REQUEST` + `termCount` + `[term...]` | `SearchReq { terms[] }` |
| **Search Reply** | `SEARCH_REPLY` + `totalHits` + `topCount` + `[(docPath, freq)...]` | `SearchRep { path → score map }` |

**Client file ownership** is tracked by namespacing each document path with its client ID on the server:  
`"client" + clientId + ":" + documentPath`

---

## Performance Results

All results measure total wall-clock time for all clients to finish indexing a dataset, and throughput in MB/s (dataset size / total time).

### Dataset 1 — 134 MB

| Clients | Socket (MB/s) | ZeroMQ (MB/s) | gRPC (MB/s) |
|---------|--------------|----------------|-------------|
| 1       | 2.84         | 9.94           | 10.64       |
| 2       | 2.82         | 17.05          | 21.08       |
| 4       | 3.15         | 23.61          | **35.85**   |

### Dataset 2 — 537 MB

| Clients | Socket (MB/s) | ZeroMQ (MB/s) | gRPC (MB/s) |
|---------|--------------|----------------|-------------|
| 1       | 2.68         | 9.06           | 8.80        |
| 2       | 2.75         | 16.50          | 17.08       |
| 4       | 2.84         | 22.46          | **28.05**   |

### Dataset 3 — 2,145 MB

| Clients | Socket Time (s) | ZeroMQ Time (s) | gRPC Time (s) | Socket (MB/s) | ZeroMQ (MB/s) | gRPC (MB/s) |
|---------|----------------|-----------------|---------------|---------------|----------------|-------------|
| 1       | 854.84         | 226.05          | 239.28        | 2.50          | 9.05           | 8.55        |
| 2       | 153.21         | 123.70          | 124.44        | 2.78          | 16.54          | 16.44       |
| 4       | 207.06         | 86.20           | **73.24**     | 2.62          | **23.73**      | **27.93**   |

---

## Head-to-Head Comparison

### Throughput Scaling (Dataset 3, 4 Clients)

```
gRPC          ████████████████████████████  27.93 MB/s
ZeroMQ        ███████████████████████       23.73 MB/s
Socket        ██                             2.62 MB/s
```

### Speedup: 4 Clients vs 1 Client (Dataset 3)

| Stack | 1 Client | 4 Clients | Speedup |
|-------|---------|-----------|---------|
| Socket Programming | 854.84 s | 207.06 s | **4.13×** |
| ZeroMQ | 226.05 s | 86.20 s | **2.62×** |
| gRPC | 239.28 s | 73.24 s | **3.27×** |

### Analysis

**Socket Programming** is by far the slowest in absolute throughput (~2.5–3.2 MB/s regardless of client count), but paradoxically shows the best *relative* parallel speedup (4.1×). This is because each client is heavily bottlenecked by the cost of manual serialization and the raw TCP stack — once you add more clients, you're just multiplying a slow baseline, so the speedup looks proportionally large. The per-client persistent thread model works fine for small concurrency but would degrade at scale.

**ZeroMQ** delivers a ~3.5–9× throughput improvement over raw sockets at 1 client, showing the real cost of hand-rolling socket management. It scales well (2.6× speedup at 4 clients) and its DEALER/ROUTER pattern makes concurrent request handling much cleaner. However, because the same binary protocol is used, the CPU cost of serialization is similar to the socket version.

**gRPC** achieves the highest peak throughput at 4 clients (35.85 MB/s on Dataset 1, 27.93 MB/s on Dataset 3). On Dataset 1 in particular, it outperforms ZeroMQ by **~52%** at 4 clients. For smaller datasets, gRPC's HTTP/2 multiplexing and efficient Protobuf encoding give it a significant edge. On Dataset 3, performance converges somewhat between gRPC and ZeroMQ (27.93 vs 23.73 MB/s) as disk I/O and index lock contention become the bottleneck rather than transport overhead. gRPC also requires the least manual infrastructure code of the three approaches.

**Summary verdict:** For a production system, gRPC offers the best combination of performance, type safety, and maintainability. ZeroMQ is a strong middle ground with more flexibility. Raw socket programming is educational but impractical for high-throughput production use.

---

## Key Design Decisions

### Client-Side Word Count
All three implementations compute word frequencies on the client, not the server. This distributes the CPU-intensive tokenization work across clients, reduces server load, and sends compact `{term: count}` maps rather than raw document text — saving bandwidth. The tradeoff is that clients carry more logic and must be redeployed if tokenization rules change.

### Stripe Locking on the Server
The shared inverted index uses 1024 mutex stripes to reduce contention when multiple clients write simultaneously. This is what allows near-linear speedup with 4 clients rather than serializing all index updates through a single lock.

### Persistent Connections
All three stacks maintain a persistent connection per client session (one TCP connection, one ZeroMQ socket, one gRPC channel) rather than reconnecting per request. This avoids handshake overhead on every INDEX message, which would be severe at the scale of millions of documents.

---

## Building & Running

### Socket Programming

```bash
cd "Socket Programming"
mkdir build && cd build
cmake ..
make
# Terminal 1
./server <port>
# Terminal 2+
./client <server_ip> <port> <dataset_path>
```

### ZeroMQ

```bash
cd ZeroMQ
mkdir build && cd build
cmake ..
make
# Terminal 1
./server <port>
# Terminal 2+
./client <server_ip> <port> <dataset_path>
```

### gRPC

```bash
cd gRPC
mkdir build && cd build
cmake ..
make
# Terminal 1
./server <port>
# Terminal 2+
./client <server_ip> <port> <dataset_path>
```

**Dependencies:** CMake 3.15+, C++17, ZeroMQ (`libzmq`), gRPC + Protobuf  

---

*Benchmarked on datasets ranging from 134 MB to 2.1 GB with 1, 2, and 4 concurrent indexing clients.*
