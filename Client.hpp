#pragma once

#include <climits>
#include <cstring>
#include <mutex>
#include <string>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

class Client
{
public:
    inline static SSL_CTX *ctx = nullptr;
    inline static BIO *connect_bio = nullptr;
    inline static SSL *ssl = nullptr;
    inline static std::mutex outputMutex;

    static int Init(const char *host, const char *port, const char *caFile = nullptr)
    {
        Shutdown();
        if (!host || !*host || !port || !*port)
            return 1;

        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx)
            return 2;
        if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1)
        {
            Shutdown();
            return 3;
        }

        const int trustLoaded = caFile
                                    ? SSL_CTX_load_verify_locations(ctx, caFile, nullptr)
                                    : SSL_CTX_set_default_verify_paths(ctx);
        if (trustLoaded != 1)
        {
            Shutdown();
            return 4;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

        connect_bio = BIO_new_ssl_connect(ctx);
        if (!connect_bio)
        {
            Shutdown();
            return 5;
        }

        const std::string endpoint = std::string(host) + ":" + port;
        if (BIO_set_conn_hostname(connect_bio, endpoint.c_str()) <= 0 ||
            BIO_get_ssl(connect_bio, &ssl) <= 0 || !ssl)
        {
            Shutdown();
            return 6;
        }

        X509_VERIFY_PARAM *verify = SSL_get0_param(ssl);
        if (X509_VERIFY_PARAM_set1_ip_asc(verify, host) != 1)
        {
            ERR_clear_error();
            if (SSL_set1_host(ssl, host) != 1 ||
                SSL_set_tlsext_host_name(ssl, host) != 1)
            {
                Shutdown();
                return 7;
            }
        }

        if (BIO_do_connect(connect_bio) <= 0)
        {
            Shutdown();
            return 8;
        }
        return 0;
    }

    static int Send(const char *data)
    {
        if (!ssl || !data)
            return 1;

        const std::size_t size = std::strlen(data);
        std::size_t sent = 0;
        while (sent < size)
        {
            const std::size_t remaining = size - sent;
            const int chunkSize = static_cast<int>(
                remaining > static_cast<std::size_t>(INT_MAX) ? INT_MAX : remaining);
            const int result = SSL_write(ssl, data + sent, chunkSize);
            if (result <= 0)
            {
                SSL_get_error(ssl, result);
                return 2;
            }
            sent += static_cast<std::size_t>(result);
        }
        return 0;
    }

    static int Receive(char *buffer, std::size_t bufferSize)
    {
        if (!ssl || !buffer || bufferSize == 0)
            return -1;

        const std::size_t capacity = bufferSize - 1;
        if (capacity == 0)
        {
            buffer[0] = '\0';
            return 0;
        }

        const int readSize = static_cast<int>(
            capacity > static_cast<std::size_t>(INT_MAX) ? INT_MAX : capacity);
        const int bytesRead = SSL_read(ssl, buffer, readSize);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            return bytesRead;
        }

        const int error = SSL_get_error(ssl, bytesRead);
        if (error == SSL_ERROR_ZERO_RETURN)
        {
            buffer[0] = '\0';
            return 0;
        }
        return -1;
    }

    static void Shutdown()
    {
        ssl = nullptr;
        if (connect_bio)
        {
            BIO_free_all(connect_bio);
            connect_bio = nullptr;
        }
        if (ctx)
        {
            SSL_CTX_free(ctx);
            ctx = nullptr;
        }
    }
};
