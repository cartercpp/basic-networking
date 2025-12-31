#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

// WSAStartup() -> getaddrinfo() -> socket() -> connect() -> send()/recv()

int main()
{
	WSAData wsa;
	bool wsaSuccess = false;
	addrinfo* serverAddress = nullptr;
	SOCKET clientSocket = INVALID_SOCKET;
	std::string filepath;
	constexpr int bufferSize = 50'000;

	auto cleanup = [&]() {
		if (clientSocket != INVALID_SOCKET)
			closesocket(clientSocket);

		if (serverAddress)
			freeaddrinfo(serverAddress);
		
		if (wsaSuccess)
			WSACleanup();
	};

	// connect to server:
	try
	{
		wsaSuccess = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
		if (!wsaSuccess)
			throw std::runtime_error{ "Failed to initialize winsock library" };

		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		const bool serverAddressSuccess
			= getaddrinfo("127.0.0.1", "8080", &hints, &serverAddress) == 0;
		if (!serverAddressSuccess)
			throw std::runtime_error{ "Failed to configure server address" };

		clientSocket = socket(
			serverAddress->ai_family,
			serverAddress->ai_socktype,
			serverAddress->ai_protocol
		);
		if (clientSocket == INVALID_SOCKET)
			throw std::runtime_error{ "Failed to create socket" };

		std::cout << "Save to> ";
		std::getline(std::cin, filepath);
		
		const bool connectSuccess
			= connect(clientSocket, serverAddress->ai_addr, serverAddress->ai_addrlen) == 0;
		if (!connectSuccess)
			throw std::runtime_error{ "Failed to connect to server" };
	}
	catch (const std::exception& e)
	{
		std::cout << "Error> " << e.what() << '\n';
		cleanup();
		return 1;
	}

	std::ofstream file{ filepath };
	while (true)
	{
		char buffer[bufferSize + 1];

		const int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);
		if (bytesReceived <= 0)
			break;
		buffer[bytesReceived] = '\0';
		
		file << buffer;
	}

	cleanup();
	std::cout << "> Received.\n";

	return 0;
}
