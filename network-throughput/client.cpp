#include <iostream>
#include <fstream>
#include <format>
#include <stdexcept>
#include <ios>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstddef>

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
	constexpr int bufferSize = 100'000;

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
			throw std::runtime_error{ "Failed to create client socket" };

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

	std::ifstream file{ "shakespeare.txt" };
	file.seekg(0, std::ios::end);
	const std::size_t fileSize = file.tellg();

	std::string fileContent(fileSize, '\0');
	file.seekg(0, std::ios::beg);
	file.read(fileContent.data(), fileSize);

	std::cout << "Application-level TCP throughput...\n";
	// send King Lear, Romeo and Juliet, Hamlet, Macbeth, and Julius Caesar 100 times:
	for (int iter = 0; iter < 100; ++iter)
	{
		std::size_t
			totalBytesSent = 0,
			totalBytesReceived = 0;

		const auto start = std::chrono::high_resolution_clock::now();
		while ((totalBytesSent < fileSize) || (totalBytesReceived < fileSize))
		{
			if (totalBytesSent < fileSize)
			{
				const int bytesSent = send(
					clientSocket,
					fileContent.data() + totalBytesSent,
					static_cast<int>(std::min<std::size_t>(fileSize - totalBytesSent, bufferSize)),
					0
				);

				if (bytesSent <= 0)
				{
					std::cerr << "Error occurred while running diagnostics...\n";
					break;
				}

				totalBytesSent += bytesSent;
			}

			if (totalBytesReceived < fileSize)
			{
				char buffer[bufferSize];

				const int bytesReceived = recv(
					clientSocket,
					buffer,
					bufferSize,
					0
				);

				if (bytesReceived <= 0)
				{
					std::cerr << "Error occurred while running diagnostics...\n";
					break;
				}

				totalBytesReceived += bytesReceived;
			}
		}
		const auto end = std::chrono::high_resolution_clock::now();

		const auto nanosecondsElapsed
			= std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
		const double secondsElapsed = nanosecondsElapsed.count() / 1'000'000'000.0;

		const auto totalBitsSent = totalBytesSent * 8;
		const double totalMegabitsSent = totalBitsSent / 1'000'000.0;

		const double megabitsPerSecond = totalMegabitsSent / secondsElapsed;

		std::cout << std::format("{:%H:%M:%S}: ~{:.1f} Mbps\n",
			std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),
			megabitsPerSecond);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
