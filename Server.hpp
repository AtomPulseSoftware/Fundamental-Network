#pragma once

#include <cstddef>
#include <cstring>
#include <mutex>
#include <vector>
#include <climits>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

class FNetS
{
public:
	inline static SSL_CTX *ctx = nullptr;
	inline static BIO *accept_bio = nullptr;
	inline static std::mutex outputMutex;

	static int Init(const char *port,
					const char *certificate = "server.crt",
					const char *privateKey = "server.key")
	{
		if (!port || !certificate || !privateKey)
			return 1;

		ctx = SSL_CTX_new(TLS_server_method());
		if (!ctx)
			return 2;

		if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1 ||
			SSL_CTX_use_certificate_chain_file(ctx, certificate) != 1 ||
			SSL_CTX_use_PrivateKey_file(ctx, privateKey, SSL_FILETYPE_PEM) != 1 ||
			SSL_CTX_check_private_key(ctx) != 1)
		{
			Shutdown();
			return 3;
		}

		accept_bio = BIO_new_accept(port);
		if (!accept_bio)
		{
			Shutdown();
			return 4;
		}

		BIO_set_bind_mode(accept_bio, BIO_BIND_REUSEADDR);
		if (BIO_do_accept(accept_bio) <= 0)
		{
			Shutdown();
			return 5;
		}

		return 0;
	}

	static SSL *Listen(int &error)
	{
		if (!ctx || !accept_bio)
		{
			error = 0;
			return nullptr;
		}
		if (BIO_do_accept(accept_bio) <= 0)
		{
			error = 1;
			return nullptr;
		}

		SSL *ssl = nullptr;
		BIO *clientBio = BIO_pop(accept_bio);
		if (!clientBio)
		{
			error = 2;
			return nullptr;
		}

		if (!ssl)
		{
			BIO_free_all(clientBio);
			error = 3;
			return nullptr;
		}

		SSL_set_bio(ssl, clientBio, clientBio);
		if (SSL_accept(ssl) <= 0)
		{
			SSL_free(ssl);
			error = 4;
			return nullptr;
		}

		BIO_get_ssl(clientBio, &ssl);
		if (!ssl)
		{
			BIO_free_all(clientBio);
			error = 5;
			return nullptr;
		}

		return ssl;
	}

	static int HandleClient(SSL *ssl, char &buffer, size_t bufferSize)
	{
		if (!ssl || !buffer || bufferSize == 0)
			return 1;
		const int capacity = static_cast<int>(
			bufferSize > static_cast<size_t>(INT_MAX)
				? INT_MAX
				: bufferSize);
		const int bytesRead = SSL_read(ssl, &buffer, capacity);
		if (bytesRead > 0)
			return 0;

		const int error = SSL_get_error(ssl, bytesRead);
		return error == SSL_ERROR_ZERO_RETURN ? 0 : 2;
	}

	static int Send(SSL *ssl, const char *data)
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

	static void Shutdown(std::vector<SSL *> &clientConnections)
	{
		for (SSL *ssl : clientConnections)
		{
			if (ssl)
			{
				SSL_shutdown(ssl);
				SSL_free(ssl);
			}
		}
		clientConnections.clear();

		if (accept_bio)
		{
			BIO_free_all(accept_bio);
			accept_bio = nullptr;
		}
		if (ctx)
		{
			SSL_CTX_free(ctx);
			ctx = nullptr;
		}
	}
};
