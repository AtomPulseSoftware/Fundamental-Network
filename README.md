# Funamental Network

A small, header-only C++ wrapper around OpenSSL for TLS client connections, server-side connection handling, and sending/receiving text data.

> **Status:** The API is intentionally small. Review the limitations below before using it in a production server.

## Features

- TLS client and server setup using OpenSSL.
- Client certificate-chain and hostname verification.
- TLS 1.2 or newer.
- Simple `Send` and `Receive` helpers.
- Header-only integration; no library build step is required.

## Requirements

- C++17 or newer (`inline static` class members are used).
- OpenSSL development headers and libraries (OpenSSL 1.1.1 or newer recommended).
- A server certificate and matching private key. The client must trust the certificate's issuing CA.

## Add to a CMake project

If OpenSSL is installed and discoverable by CMake, add the repository headers to your project and link OpenSSL:

```cmake
find_package(OpenSSL REQUIRED)

add_executable(my_app main.cpp)
target_compile_features(my_app PRIVATE cxx_std_17)
target_include_directories(my_app PRIVATE /path/to/Funamental-Network)
target_link_libraries(my_app PRIVATE OpenSSL::SSL OpenSSL::Crypto)
```

Replace `/path/to/Funamental-Network` with the directory containing `Client.hpp` and `Server.hpp`. This repository currently provides no CMake project of its own; the snippet shows how to consume the headers from another project.

## Client example

`Client::Init` connects to the host and completes the TLS handshake. It verifies both the certificate chain and the host name. By default, OpenSSL's system trust store is used; pass a CA file when needed.

```cpp
#include "Client.hpp"

#include <array>
#include <iostream>
#include <string>

int main()
{
    // Use a DNS name that appears in the server certificate.
    if (Client::Init("example.com", "443") != 0)
    {
        std::cerr << "TLS connection failed\n";
        return 1;
    }

    if (Client::Send("Hello over TLS") != 0)
    {
        std::cerr << "Send failed\n";
        Client::Shutdown();
        return 1;
    }

    std::array<char, 1024> buffer{};
    const int bytesRead = Client::Receive(buffer.data(), buffer.size());
    if (bytesRead > 0)
        std::cout << "Received: " << std::string(buffer.data(), bytesRead) << '\n';
    else if (bytesRead == 0)
        std::cout << "The server closed the connection\n";
    else
        std::cerr << "Receive failed\n";

    Client::Shutdown();
    return 0;
}
```

For a private CA, pass its certificate file as the third argument, for example `Client::Init("server.example", "443", "root-ca.pem")`.

## Server setup example

`FNetS` is the server class name in `Server.hpp`. Initialize it with a listening port and a PEM certificate chain/private key, then call `Listen` to accept TLS clients:

```cpp
#include "Server.hpp"

#include <vector>

int main()
{
    if (FNetS::Init("4433", "server.crt", "server.key") != 0)
        return 1;

    std::vector<SSL*> clients;
    int error = 0;

    // Listen blocks while accepting connections.
    const bool success = FNetS::Listen(&clients, error, true);

    // Handle accepted SSL* connections before shutting the server down.
    // See the API notes below for current limitations around client handling.

    FNetS::Shutdown();
    return success ? 0 : 1;
}
```

The certificate and private key must match. Do not commit private keys to source control. For local testing, use a test certificate and configure the client to trust its CA.

## API overview

### `Client.hpp`

- `Client::Init(host, port, caFile = nullptr)` — connect and establish a verified TLS session; returns `0` on success.
- `Client::Send(data)` — send a null-terminated string; returns `0` on success.
- `Client::Receive(buffer, bufferSize)` — read up to `bufferSize - 1` bytes and append a null terminator. Returns the number of bytes read, `0` when the peer closes the connection, or `-1` on error.
- `Client::Shutdown()` — release the connection and TLS context.

### `Server.hpp`

- `FNetS::Init(port, certificate, privateKey)` — initialize TLS and bind the listening port; returns `0` on success.
- `FNetS::Listen(&clients, error, run)` — block accepting clients and append successfully handshaken `SSL*` connections to the vector.
- `FNetS::Send(ssl, data)` — send a null-terminated string to one connected client; returns `0` on success.
- `FNetS::HandleClient(ssl, buffer, bufferSize)` — read from a client until it disconnects. **Currently, received bytes are discarded rather than copied to `buffer`.**
- `FNetS::Shutdown()` — release the listening socket and TLS context.

## Important limitations

- `Send` and `Receive` work with byte streams, not messages: one send is not guaranteed to correspond to one receive. Add your own framing (for example, a length prefix or newline-delimited protocol) when exchanging multiple messages.
- The current `FNetS::HandleClient` discards incoming data. It is not yet a message callback or echo handler.
- `FNetS::Listen` takes `run` by value and blocks during accept, so changing the caller's flag later will not stop the loop. The current API does not provide a complete graceful-stop mechanism.
- If you use `Listen` on a separate thread, protect shared access to the clients vector yourself. Coordinate TLS reads/writes and connection cleanup so the same `SSL*` is not freed while another thread is using it.
- `Client` stores its connection in static members, so it supports one active client connection per process.
- The `Send` functions accept null-terminated strings; they are not suitable for arbitrary binary data containing null bytes.

## License

MIT. See [LICENSE](LICENSE).
