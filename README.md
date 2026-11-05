# Fundamental Network

A small, header-only C++ wrapper around OpenSSL for TLS client connections and server-side connection handling.

## Features

- TLS 1.2 or newer.
- Client peer certificate-chain verification using system trust paths by default or a supplied CA file.
- Header-only integration; no library build step is provided.
- Text-oriented send and receive helpers.

## Requirements

- C++17 or newer.
- OpenSSL development headers and libraries (OpenSSL 1.1.1 or newer).
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

Replace `/path/to/Fundamental-Network` with the directory containing `Client.hpp` and `Server.hpp`. The repository does not currently include its own CMake project



For a private CA, initialize with `Client::Init("server.example", "443", "root-ca.pem")`.

## Server API

`FNetS` in `Server.hpp` exposes server context setup, a `Listen(int &error)` method, client read/write helpers, and shutdown:

```cpp
FNetS::Init("4433", "server.crt", "server.key");
```

The certificate and private key must match. The defaults are `server.crt` and `server.key`. Do not commit private keys to source control.

## API overview

### `Client.hpp`

- `Client::Init(const char *ip, const char *port, const char *caFile = nullptr)` — initialize TLS, connect, and verify the peer; returns `0` on success and a nonzero error code on failure.
- `Client::Send(const char *data)` — send a null-terminated string; returns `0` on success, `1` for invalid/uninitialized input, or `2` for a TLS write failure.
- `Client::Receive(char *buffer, std::size_t bufferSize)` — read up to `bufferSize - 1` bytes and append a null terminator. Returns the byte count, `0` when the peer closes the TLS connection (also returns `0` if the buffer size is `1`), or `-1` for invalid input or a TLS read failure.
- `Client::Shutdown()` — release the client connection BIO and TLS context.

### `Server.hpp`

- `FNetS::Init(const char *port, const char *certificate = "server.crt", const char *privateKey = "server.key")` — configure TLS and bind the listening port; returns `0` on success and a nonzero error code on failure.
- `FNetS::Listen(int &error)` — intended to accept a connection and return its `SSL*`; currently always returns `nullptr` because the local SSL pointer is never created. `error` is set to `3` on that path.
- `FNetS::HandleClient(SSL *ssl, char &buffer, size_t bufferSize)` — currently unusable as a general buffer reader. It accepts a reference to one `char`, checks the character value as though it were a pointer, and does not append a null terminator.
- `FNetS::Send(SSL *ssl, const char *data)` — intended to send a null-terminated string on an SSL connection; returns `0` on success, `1` for invalid input, or `2` for a TLS write failure.
- `FNetS::Shutdown(std::vector<SSL *> &clientConnections)` — shuts down and frees the SSL connections in the supplied vector, then releases the listening BIO and TLS context. The caller must track owned connections and invoke this overload.

## Examples

### Client example

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
        std::cout << "The server closed the connection (or the buffer has no data capacity)\n";
    else
        std::cerr << "Receive failed\n";

    Client::Shutdown();
    return 0;
}
```

### Server example

```cpp
#include "Server.hpp"

#include <iostream>
#include <string>
#include <vector>

#include <openssl/ssl.h>

int main(int argc, char *argv[])
{
    const char *port = argc > 1 ? argv[1] : "4433";
    const char *certificate = argc > 2 ? argv[2] : "server.crt";
    const char *privateKey = argc > 3 ? argv[3] : "server.key";

    const int initResult = FNetS::Init(port, certificate, privateKey);
    if (initResult != 0)
    {
        std::cerr << "FNetS::Init failed with error " << initResult << '\n';
        ERR_print_errors_fp(stderr);
        return 1;
    }

    std::cout << "Waiting for one TLS client on port " << port << "...\n";

    int listenError = 0;
    SSL *client = FNetS::Listen(listenError);
    if (!client)
    {
        std::cerr << "FNetS::Listen failed with error " << listenError << '\n';
        ERR_print_errors_fp(stderr);
        std::vector<SSL *> noClients;
        FNetS::Shutdown(noClients);
        return 1;
    }

    std::vector<SSL *> clients{client};
    std::string request;
    char buffer[1024];

    for (;;)
    {
        const int bytesRead = FNetS::HandleClient(client, buffer, sizeof(buffer));
        if (bytesRead < 0)
        {
            std::cerr << "TLS read failed\n";
            ERR_print_errors_fp(stderr);
            FNetS::Shutdown(clients);
            return 1;
        }
        if (bytesRead == 0)
            break;

        request.append(buffer, static_cast<std::size_t>(bytesRead));
        if (request.find('\n') != std::string::npos)
            break;
    }

    std::cout << "Received: " << request;
    if (request.empty() || request.back() != '\n')
        std::cout << '\n';

    const char response[] = "Message received\n";
    if (FNetS::Send(client, response) != 0)
    {
        std::cerr << "TLS write failed\n";
        ERR_print_errors_fp(stderr);
        FNetS::Shutdown(clients);
        return 1;
    }

    FNetS::Shutdown(clients);
    return 0;
}
```
