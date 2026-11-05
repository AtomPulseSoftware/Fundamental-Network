# Fundamental Network

A small, header-only C++ wrapper around OpenSSL for TLS client connections and server-side connection handling.

> **Status:** The client implementation provides a basic blocking TLS connection, text send, and receive API. The server implementation is **not currently usable**: `FNetS::Listen` always returns `nullptr` because it checks a local `SSL*` that is initialized to `nullptr` and never creates an `SSL` object. `HandleClient` also has an incorrect buffer parameter and validation. See [Known server issues](#known-server-issues) before using the server API.

## Features

- TLS 1.2 or newer.
- Client peer certificate-chain verification using system trust paths by default or a supplied CA file.
- Header-only integration; no library build step is provided.
- Text-oriented send and receive helpers.

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
        std::cout << "The server closed the connection (or the buffer has no data capacity)\n";
    else
        std::cerr << "Receive failed\n";

    Client::Shutdown();
    return 0;
}
```

For a private CA, initialize with `Client::Init("server.example", "443", "root-ca.pem")`.

## Server API

`FNetS` in `Server.hpp` exposes server context setup, a `Listen(int &error)` method, client read/write helpers, and shutdown:

```cpp
FNetS::Init("4433", "server.crt", "server.key");
```

The certificate and private key must match. The defaults are `server.crt` and `server.key`. Do not commit private keys to source control.

**The server API is not ready for application use.** In the current implementation, `Listen` cannot return an accepted connection, and `HandleClient` does not provide a usable buffer interface. Details are listed below. The signatures are documented for reference only; do not rely on successful server-side client handling until these defects are fixed.

## API overview

### `Client.hpp`

- `Client::Init(const char *ip, const char *port, const char *caFile = nullptr)` — initialize TLS, connect, and verify the peer; returns `0` on success and a nonzero error code on failure.
- `Client::Send(const char *data)` — send a null-terminated string; returns `0` on success, `1` for invalid/uninitialized input, or `2` for a TLS write failure.
- `Client::Receive(char *buffer, std::size_t bufferSize)` — read up to `bufferSize - 1` bytes and append a null terminator. Returns the byte count, `0` when the peer closes the TLS connection (also returns `0` if the buffer size is `1`), or `-1` for invalid input or a TLS read failure.
- `Client::Shutdown()` — release the client connection BIO and TLS context.

### `Server.hpp` (currently defective)

- `FNetS::Init(const char *port, const char *certificate = "server.crt", const char *privateKey = "server.key")` — configure TLS and bind the listening port; returns `0` on success and a nonzero error code on failure.
- `FNetS::Listen(int &error)` — intended to accept a connection and return its `SSL*`; currently always returns `nullptr` because the local SSL pointer is never created. `error` is set to `3` on that path.
- `FNetS::HandleClient(SSL *ssl, char &buffer, size_t bufferSize)` — currently unusable as a general buffer reader. It accepts a reference to one `char`, checks the character value as though it were a pointer, and does not append a null terminator.
- `FNetS::Send(SSL *ssl, const char *data)` — intended to send a null-terminated string on an SSL connection; returns `0` on success, `1` for invalid input, or `2` for a TLS write failure.
- `FNetS::Shutdown(std::vector<SSL *> &clientConnections)` — shuts down and frees the SSL connections in the supplied vector, then releases the listening BIO and TLS context. The caller must track owned connections and invoke this overload.

## Known server issues

1. **`Listen` never creates an `SSL` object.** It initializes `SSL *ssl = nullptr` and immediately checks `if (!ssl)`, so every accepted socket is freed and the function returns `nullptr` with error code `3`. The later `SSL_set_bio`, handshake, and `BIO_get_ssl` code cannot be reached. The accept/TLS BIO ownership flow needs to be corrected as part of fixing this.
2. **`HandleClient` has the wrong parameter type and validation.** A `char&` cannot represent a caller-provided character buffer. Also, `!buffer` tests whether the character is NUL, not whether a buffer pointer is null. Change the API to accept a writable pointer (or an appropriate span), validate its capacity, read into it, and document whether/how it terminates text.
3. **Repeated `FNetS::Init` calls can overwrite existing static resources.** Call `Shutdown` or reject reinitialization before assigning new context/BIO pointers.
4. **`Listen` reports an uninitialized server as `error = 0`.** That value conventionally indicates success; use a nonzero error code for the invalid state.

Fix and test these issues before using the server API. In particular, test successful accept/handshake, failed handshakes, multiple sequential clients, and cleanup after both success and failure.

## General limitations

- TLS is a byte stream: one call to `Send` is not guaranteed to correspond to one `Receive`. Add message framing, such as a length prefix or newline delimiter, in your application.
- The send helpers use `strlen` and accept null-terminated strings; they are not suitable for arbitrary binary data containing null bytes.
- `Client` stores its connection in static members and supports only one active client connection per process.
- The `outputMutex` members are declared but not used; callers must coordinate concurrent access themselves.
- The current examples use blocking OpenSSL operations and do not provide timeouts or a nonblocking event loop.

## License

MIT. See [LICENSE](LICENSE).
