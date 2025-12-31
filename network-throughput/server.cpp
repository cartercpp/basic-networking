server.cpp
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

// WSAStartup() -> getaddrinfo() -> socket() -> bind() -> listen() -> accept() -> recv()/send()

int main()
{
	WSAData wsa;
	bool wsaSuccess = false;
	addrinfo* serverAddress = nullptr;
	SOCKET
		serverSocket = INVALID_SOCKET,
		clientSocket = INVALID_SOCKET;
	constexpr int bufferSize = 100'000;

	auto cleanup = [&]() {
		if (clientSocket != INVALID_SOCKET)
			closesocket(clientSocket);

		if (serverSocket != INVALID_SOCKET)
			closesocket(serverSocket);

		if (serverAddress)
			freeaddrinfo(serverAddress);

		if (wsaSuccess)
			WSACleanup();
		};

	// set up server:
	try
	{
		wsaSuccess = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
		if (!wsaSuccess)
			throw std::runtime_error{ "Failed to initialize winsock library" };

		addrinfo hints{};
		hints.ai_family = AF_INET; // ipv4
		hints.ai_socktype = SOCK_STREAM; // tcp
		hints.ai_flags = AI_PASSIVE; // server

		const bool serverAddressSuccess
			= getaddrinfo(nullptr, "8080", &hints, &serverAddress) == 0;
		if (!serverAddressSuccess)
			throw std::runtime_error{ "Failed to configure server socket address" };

		serverSocket = socket(
			serverAddress->ai_family,
			serverAddress->ai_socktype,
			serverAddress->ai_protocol
		);
		if (serverSocket == INVALID_SOCKET)
			throw std::runtime_error{ "Failed to create server socket" };

		const bool bindSuccess
			= bind(serverSocket, serverAddress->ai_addr, serverAddress->ai_addrlen) == 0;
		if (!bindSuccess)
			throw std::runtime_error{ "Failed to bind server socket to server" };

		const bool listenSuccess = listen(serverSocket, SOMAXCONN) == 0;
		if (!listenSuccess)
			throw std::runtime_error{ "Failed to listen for incoming connections" };

		clientSocket = accept(serverSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
			throw std::runtime_error{ "Failed to accept incoming connection" };
	}
	catch (const std::exception& e)
	{
		std::cout << "Error> " << e.what() << '\n';
		cleanup();
		return 1;
	}

	// echo:
	while (true)
	{
		char buffer[bufferSize];
		const int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

		if (bytesReceived <= 0)
			break;

		int totalBytesSent = 0;
		bool sendFailed = false;

		while (totalBytesSent < bytesReceived)
		{
			const int bytesSent = send(
				clientSocket,
				buffer + totalBytesSent,
				bytesReceived - totalBytesSent,
				0
			);

			if (bytesSent <= 0)
			{
				sendFailed = true;
				break;
			}

			totalBytesSent += bytesSent;
		}

		if (sendFailed)
			break;
	}

	cleanup();
	std::cin.get();
	return 0;
}
