# FENRIS - Secure Networked File System

[![Ubuntu Build and Test](https://github.com/std-fenris/fenris/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/std-fenris/fenris/actions/workflows/ubuntu.yml)

Fenris is a secure networked file system (NFS) implementation with encryption, concurrency management, and caching. It provides a client-server architecture for secure file operations over a network.

## Features

- **Secure File Operations**: Read, write, append, delete, and list files with end-to-end encryption
- **AES Encryption**: Files are encrypted during transfer using AES-GCM for confidentiality and integrity
- **ECDH Key Exchange**: Secure key exchange between client and server
- **Concurrency Management**: Proper synchronization mechanisms to handle concurrent file access
- **Performance Caching**: Implements caching to improve file access performance
- **Cross-Platform**: Works on Linux and other POSIX compliant systems

## Project Structure

```
fenris/
├── include/          # Header files
│   ├── client/       # Client-specific headers
│   ├── common/       # Shared functionality headers
│   ├── server/       # Server-specific headers
│   └── external/     # Third-party libraries
├── src/              # Source files
│   ├── client/       # Client implementation
│   ├── common/       # Shared functionality implementation
│   └── server/       # Server implementation
├── tests/            # Test suite
│   └── unittests/    # Unit tests
└── cmake/            # CMake utilities
```

## Requirements

- C++20 compatible compiler (GCC 10+ or Clang 10+)
- CMake 3.5+
- CryptoPP, SPDLog, and zlib (automatically handled by CMake)
- Git (for cloning the repository with submodules)

## Building from Source

### Cloning the Repository

```bash
git clone --recursive https://github.com/std-fenris/fenris.git
cd fenris
```

If you already cloned the repository without `--recursive`, update the submodules:

```bash
git submodule update --init --recursive
```

### Building with CMake

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

For debug build:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUNIT_TESTING=ON ..
make -j$(nproc)
```

## Running Tests

```bash
cd build
ctest --verbose
```

## Installation

After building, install to your system:

```bash
sudo make install
```

## Usage

### Starting the Server

```bash
fenris_server [options]
```

Options:
- `--port <port>` : Specify the port to listen on (default: 9876)
- `--dir <path>` : Specify the root directory for file operations
- `--log-level <level>` : Set log level (trace, debug, info, warn, error, critical)

### Using the Client

```bash
fenris_client [options] [commands]
```

Options:
- `--server <address>` : Server address (default: localhost)
- `--port <port>` : Server port (default: 9876)
- `--log-level <level>` : Set log level

Commands:
- `create <filename>` : Create a new file
- `read <filename>` : Read file contents
- `upload <filename>` : Upload a file
- `download <filename>` : Download a file
- `write <filename> <content>` : Write to file
- `append <filename> <content>` : Append to file
- `delete <filename>` : Delete a file
- `list <directory>` : List directory contents
- `mkdir <directory>` : Create directory
- `rmdir <directory>` : Remove directory

## Implementation Details

### Encryption

Fenris uses industry-standard encryption techniques:
- AES-GCM for file encryption (providing both confidentiality and integrity)
- ECDH (Elliptic Curve Diffie-Hellman) for secure key exchange
- SHA-256 for key derivation

### Concurrency Management

The server implements proper synchronization to handle concurrent file operations:
- File-level locking for read/write operations
- Fine-grained synchronization to maximize throughput
- Deadlock prevention mechanisms

### Caching Mechanism

- Further details on caching strategies will be added soon.

## Performance Considerations

- **Network Overhead**: Encryption adds minimal overhead to network transfers
- **Concurrency**: Multiple clients can access different files simultaneously without contention
- **Caching**: Repeated access to the same files benefits from caching, significantly reducing latency

