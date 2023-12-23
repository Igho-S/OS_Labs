#include <iostream>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm>
#include <thread>
#include <mutex>

std::mutex signalMutex;
volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int signal) {
    wasSigHup = 1;
}

int main() {
    int superSocket, newSocket;
    std::vector<int> clientSockets;
    struct sockaddr_in serverAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr_in);
    std::vector<std::thread> clientThreads;
    struct sigaction sa;

    // To create a socket for listening
    superSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (superSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // Here to bind the socket to the specified port
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(25252);
    if (bind(superSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to bind to port" << std::endl;
        close(superSocket);
        return 1;
    }

    // To listen for incoming connections
    if (listen(superSocket, 5) == -1) {
        std::cerr << "Failed to listen for connections" << std::endl;
        close(superSocket);
        return 1;
    }

    std::cout << "Server listening on port 25252" << std::endl;

    // Here, to register signal handler for SIGHUP
    // Sets the handler to sigHupHandler, and ensures that system calls are restarted if interrupted by this signal.
    sigaction(SIGHUP, nullptr, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, nullptr);

    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(superSocket, &fds); // clears the file descriptor set fds and adds the superSocket to it.

        int maxFd = superSocket; // Update maxFd based on established connections

        {
            std::lock_guard<std::mutex> lock(signalMutex);
            if (wasSigHup) {
                std::cout << "SIGHUP signal was processed" << std::endl;
                wasSigHup = 0;
            }
        }

        if (select(maxFd + 1, &fds, nullptr, nullptr, nullptr) == -1) {
            std::cerr << "select error" << std::endl;
            continue;
        }

        if (FD_ISSET(superSocket, &fds)) {
            newSocket = accept(superSocket, nullptr, nullptr);

            // Here, to add the new client socket to the vector
            clientSockets.push_back(newSocket);

            std::cout << "New client connected." << std::endl;

            // Create a new thread for each client
            clientThreads.emplace_back([newSocket, &clientSockets]() {
                char buffer[1024];
                int bytesRead;
                while ((bytesRead = recv(newSocket, buffer, sizeof(buffer), 0)) > 0) {
                    // Process and handle the received data
                    send(newSocket, buffer, bytesRead, 0);
                }

                // To delete the disconnected client socket from the vector
                clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), newSocket), clientSockets.end());

                std::cout << "Client disconnected." << std::endl;

                close(newSocket); // Close the client socket
            });
        }

        // Cleanup finished threads
        clientThreads.erase(std::remove_if(clientThreads.begin(), clientThreads.end(),
                                           [](const std::thread& t) { return !t.joinable(); }),
                            clientThreads.end());
    }

    close(superSocket);

    return 0;
}
