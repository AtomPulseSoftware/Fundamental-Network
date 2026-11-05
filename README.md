# Fundamental Network

A small, header-only C++ wrapper around OpenSSL for establishing TLS client connections and accepting TLS connections on the server side.

> **Status:** The client API can connect, send, and receive null-terminated text. The server API currently has implementation defects: `Listen` does not return the accepted `SSL*` to its caller, and `HandleClient` calls `SSL_read` with a null buffer. Do not rely on these server methods for application traffic until they are fixed.

## Features

- TLS client and server context setup with OpenSSL.
- TLS 1.2 or newer.
- Client certificate-chain and host verification, using system trust paths by default or a supplied CA file.
- Helpers for sending and receiving null-terminated text on the client.
- Header-only integration; no library build step is provided.

## Requirements

- C++17 or newer (`inline static` class members are used).
- OpenSSL development headers and libraries (OpenSSL 1.1.1 or newer recommended).
- For a server, a PEM certificate chain and its matching private key.

## Add to a CMake project

If OpenSSL is installed and discoverable by CMake, add this repository's directory to your include path and link OpenSSL:

```cmake
find_package(OpenSSL REQUIRED)

add_executable(my_app main.cpp)
target_compile_features(my_app PRIVATE cxx_std_17)
target_include_directories(my_app PRIVATE /path/to/Fundamental-Network)
target_link_libraries(my_app PRIVATE OpenSSL::SSL OpenSSL::Crypto)
```

Replace `/path/to/Fundamental-Network` with the directory containing `Client.hpp` and `Server.hpp`. The repository does not currently include its own CMake project.

## Client example

`Client::Init` connects to the host and completes a TLS handshake, verifying the certificate chain and host name. By default, it uses OpenSSL's system trust paths. For a private CA, pass the CA certificate file as the third argument.

```cpp
#include "Client.hpp"

#include <array>
#include <iostream>
#include <string>

int main()
{
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

For a private CA, initialize with `Client::Init("server.example", "443", "root-ca.pem")`.

## Server API status

`FNetS` in `Server.hpp` provides server context and listening BIO setup:

```cpp
FNetS::Init("4433", "server.crt", "server.key");
```

The certificate and private key must match. The defaults are `server.crt` and `server.key`. Do not commit private keys to source control.

The current server methods are **not ready for reliable client handling**:

- `FNetS::Listen(SSL *client)` accepts and handshakes a connection, but `client` is passed by value. Assigning the new `SSL*` inside the function does not make it available to the caller. The accepted connection is therefore not exposed through this API.
- `FNetS::HandleClient(SSL *ssl, char *buffer, size_t bufferSize)` currently passes a null pointer to `SSL_read` instead of `buffer`. Do not call it; it cannot safely receive data as written.
- `FNetS::Shutdown()` releases the listening BIO and TLS context. No complete accepted-client ownership or graceful-stop API is currently provided.

These limitations should be fixed in the implementation before using the server API in an application.

## API overview

### `Client.hpp`

- `Client::Init(const char *host, const char *port, const char *caFile = nullptr)` — initialize TLS, connect, and verify the peer; returns `0` on success and a nonzero error code on failure.
- `Client::Send(const char *data)` — send a null-terminated string; returns `0` on success, `1` for invalid/uninitialized input, or `2` for a TLS write failure.
- `Client::Receive(char *buffer, std::size_t bufferSize)` — read up to `bufferSize - 1` bytes and append a null terminator. Returns the byte count, `0` when the peer closes the TLS connection, or `-1` for invalid input or a read failure.
- `Client::Shutdown()` — release the connection and TLS context.

### `Server.hpp`

- `FNetS::Init(const char *port, const char *certificate = "server.crt", const char *privateKey = "server.key")` — configure TLS and bind the listening port; returns `0` on success and a nonzero error code on failure.
- `FNetS::Listen(SSL *client)` — attempts to accept and complete a TLS handshake, but does not currently return the accepted connection to the caller (see limitations above).
- `FNetS::HandleClient(SSL *ssl, char *buffer, size_t bufferSize)` — currently unsafe because it passes a null read buffer (see limitations above).
- `FNetS::Send(SSL *ssl, const char *data)` — send a null-terminated string on an SSL connection; returns `0` on success, `1` for invalid input, or `2` for a TLS write failure.
- `FNetS::Shutdown()` — release the listening BIO and TLS context.

## General limitations

- TLS is a byte stream: one call to `Send` is not guaranteed to correspond to one `Receive`. Add message framing, such as a length prefix or newline delimiter, in your application.
- The send helpers use `strlen` and accept null-terminated strings; they are not suitable for arbitrary binary data containing null bytes.
- `Client` stores its connection in static members and supports only one active client connection per process.
- The server's `outputMutex` and client's `outputMutex` are declared but are not used by the current implementation; callers must coordinate concurrent access themselves.

## License

MIT. See [LICENSE](LICENSE).
