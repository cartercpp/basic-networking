#include <iostream>
#include <fstream>
#include <ios>
#include <string>
#include <format>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <stop_token>
#include <mutex>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

// WSAStartup() -> getaddrinfo() -> socket() -> bind() -> listen() -> accept() -> send()/recv()

int main()
{
	WSAData wsa;
	bool wsaSuccess = false;
	addrinfo* serverAddress = nullptr;
	SOCKET serverSocket = INVALID_SOCKET;
	std::string filepath;
	std::jthread fileDistributionThread;
	constexpr int bufferSize = 50'000;

	auto cleanup = [&]() {
		if (serverSocket != INVALID_SOCKET)
			closesocket(serverSocket);

		if (serverAddress)
			freeaddrinfo(serverAddress);

		if (wsaSuccess)
			WSACleanup();
		};

	//set up server:
	try
	{
		wsaSuccess = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
		if (!wsaSuccess)
			throw std::runtime_error{ "Failed to initialize winsock library" };

		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;

		const bool serverAddressSuccess = getaddrinfo(nullptr, "8080", &hints, &serverAddress) == 0;
		if (!serverAddressSuccess)
			throw std::runtime_error{ "Failed to configure server address" };

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
			throw std::runtime_error{ "Failed to bind server socket to server address" };

		const bool listenSuccess = listen(serverSocket, SOMAXCONN) == 0;
		if (!listenSuccess)
			throw std::runtime_error{ "Failed to listen for incoming connections" };

		std::cout << "Send file> ";
		std::getline(std::cin, filepath);
		if (!std::ifstream{ filepath })
			throw std::runtime_error{ "File doesn't exist" };
	}
	catch (const std::exception& e)
	{
		std::cout << "Error> " << e.what() << '\n';
		cleanup();
		return 1;
	}

	std::cout << "Hit enter to take server offline...\n";

	fileDistributionThread = std::jthread{ [&](std::stop_token st) {
		while (!st.stop_requested())
		{
			fd_set readFds{};
			FD_ZERO(&readFds);
			FD_SET(serverSocket, &readFds);

			timeval tv{.tv_sec = 0, .tv_usec = 250'000 }; //wait 0.25 seconds
			const int ready = select(0, &readFds, nullptr, nullptr, &tv);

			if ((ready > 0) && FD_ISSET(serverSocket, &readFds))
			{
				sockaddr_storage clientAddress{};
				int clientSize = static_cast<int>(sizeof(clientAddress));

				SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientSize);
				if (clientSocket == INVALID_SOCKET)
					continue;

				char host[NI_MAXHOST];
				const bool getnameinfoSuccess = getnameinfo(
					(sockaddr*)&clientAddress, clientSize,
					host, sizeof(host),
					nullptr, 0,
					NI_NUMERICHOST
				) == 0;
				if (!getnameinfoSuccess)
				{
					closesocket(clientSocket);
					continue;
				}

				std::ifstream file{ filepath };
				file.seekg(0, std::ios::end);
				const std::size_t fileSize = file.tellg();

				std::size_t fileIndex = 0;
				while (fileIndex < fileSize)
				{
					file.seekg(fileIndex, std::ios::beg);
					char buffer[bufferSize];
					const int strSize
						= static_cast<int>(std::min<std::size_t>(fileSize - fileIndex, bufferSize));
					file.read(buffer, strSize);

					const int bytesSent = send(
						clientSocket,
						buffer,
						strSize,
						0
					);

					if (bytesSent <= 0)
						break;

					fileIndex += bytesSent;
				}

				std::cout << std::format("Sent {} to {} at {:%H:%M:%S}\n",
					filepath, std::string{ host },
					std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));

				closesocket(clientSocket);
			}
		}
	} };

	std::cin.get();

	fileDistributionThread.request_stop();
	if (fileDistributionThread.joinable())
		fileDistributionThread.join();
	cleanup();

	return 0;
}
