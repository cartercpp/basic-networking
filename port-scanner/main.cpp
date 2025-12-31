#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <thread>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

// WSAStartup() -> getaddrinfo() -> socket() -> connect()

int main()
{
	WSAData wsa;
	bool wsaSuccess = false;

	try
	{
		wsaSuccess = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
		if (!wsaSuccess)
			throw std::runtime_error{ "Failed to initialize winsock library" };

		std::uint16_t
			startPort,
			endPort;

		std::cout << "Start port> ";
		std::cin >> startPort;
		std::cout << "End port> ";
		std::cin >> endPort;
		std::cout << std::string(35, '-') << '\n';
		
		std::mutex mtx;
		const unsigned int threadCount = std::thread::hardware_concurrency();

		auto func = [&, increment = threadCount](unsigned int portNumber) {
			addrinfo hints{};
			hints.ai_family = AF_INET; // ipv4
			hints.ai_socktype = SOCK_STREAM; // tcp

			while (portNumber <= endPort)
			{
				const std::string portStr{ std::to_string(portNumber) };
				addrinfo* serverAddress = nullptr;
				SOCKET clientSocket = INVALID_SOCKET;

				auto cleanup_and_increment = [&]() {
					if (clientSocket != INVALID_SOCKET)
						closesocket(clientSocket);

					if (serverAddress)
						freeaddrinfo(serverAddress);

					portNumber += increment;
				};

				const bool serverAddressSuccess
					= getaddrinfo("127.0.0.1", portStr.data(), &hints, &serverAddress) == 0;
				if (!serverAddressSuccess)
				{
					cleanup_and_increment();
					continue;
				}

				clientSocket = socket(
					serverAddress->ai_family,
					serverAddress->ai_socktype,
					serverAddress->ai_protocol
				);
				if (clientSocket == INVALID_SOCKET)
				{
					cleanup_and_increment();
					continue;
				}

				const bool connectSuccess
					= connect(clientSocket, serverAddress->ai_addr, serverAddress->ai_addrlen) == 0;
				
				if (connectSuccess)
				{
					std::lock_guard<std::mutex> lck{ mtx };
					std::cout << std::format("> Connected to port {}.\n", portNumber);
				}

				cleanup_and_increment();
			}
		};

		//   thread1:         thread2:        thread3:        thread4:

		// startPort + 0	startPort + 1	startPort + 2	startPort + 3
		// startPort + 4	startPort + 5	startPort + 6	startPort + 7
		// startPort + 8	startPort + 9	startPort + 10	startPort + 11
		// startPort + 12	startPort + 13	startPort + 14	startPort + 15
		//	   ...               ...             ...             ...
		//	   ...               ...             ...             ...
		//	   ...               ...             ...             ...

		std::vector<std::jthread> threads(threadCount);
		for (unsigned int i = 0; i < threadCount; ++i)
			threads[i] = std::jthread{ func, startPort + i };
	}
	catch (const std::exception& e)
	{
		std::cout << "Error> " << e.what() << '\n';
	}

	if (wsaSuccess)
		WSACleanup();

	return 0;
}
