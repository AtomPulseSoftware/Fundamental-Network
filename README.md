Funamental Network

A small library that make the work with OpenSSL easier.

Instructions:
	Use the CMake and add "find_package(OpenSSL REQUIRED) target_link_libraries(your_project_name PRIVATE OpenSSL::SSL OpenSSL::Crypto)"
	To make server, use Server.hpp
		To init, use Server::Init();
		To handle client, use thread and Server::HandleClient(client_ssl);
		To listen to income connection, use thread and Server::Listen(clients_vector, error_code, running);
		To send a client a message, use Server::Send(client_ssl, message);
	To connect to the server, use Client.hpp
		To init, use Client::Init();
		To recieve, use thread and Client::Recieve(buffer, buffer_size);
		To send, use Client::Send(message);
		
