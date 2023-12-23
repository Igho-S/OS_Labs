#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <csignal>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm>


//Хаззан Омотола 932024

#define PORT 25252

std::mutex signalMutex;
volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int signal) {
    wasSigHup = 1;
}

void handleClient(int clientSocket) {
    char buffer[1024];
    int bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        // To process and handle the received data
        send(clientSocket, buffer, bytesRead, 0);
    }

    close(clientSocket);
}

int main() {
    int serverSocket, newSocket;
    std::vector<int> clientSockets;
    struct sockaddr_in serverAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr_in);
    fd_set fds; // declares an fd_set structure that will be used with pselect to monitor file descriptors.
    struct sigaction sa;

    // To create a socket for listening
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // Here to bind the socket to the specified port
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to bind to port" << std::endl;
        close(serverSocket);
        return 1;
    }

    // To listen for incoming connections
    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Failed to listen for connections" << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // Here, to register signal handler for SIGHUP
    // Sets the handler to sigHupHandler, and ensures that system calls are restarted if interrupted by this signal.
    sigaction(SIGHUP, nullptr, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, nullptr);

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);

    while (true) {
        FD_ZERO(&fds);
        FD_SET(serverSocket, &fds);

        int maxFd = serverSocket;

        int result = pselect(maxFd + 1, &fds, nullptr, nullptr, nullptr, &origMask);

        if (result == -1) {
            if (errno == EINTR) {
                std::cout << "Received SIGHUP signal" << std::endl;
                {
                    std::lock_guard<std::mutex> lock(signalMutex);
                    wasSigHup = 0;
                }
            } else {
                std::cerr << "pselect error" << std::endl;
            }
        }

        if (wasSigHup) {
            std::cout << "SIGHUP signal was processed" << std::endl;
        }

        if (FD_ISSET(serverSocket, &fds)) {
            newSocket = accept(serverSocket, nullptr, nullptr);

            if (newSocket == -1) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            // Here, to add the new client socket to the vector
            clientSockets.push_back(newSocket);

            std::cout << "New client connected." << std::endl;

            // Here, to handle the new client in a separate thread
            std::thread(handleClient, newSocket).detach();
        }
    }

    close(serverSocket);

    return 0;
}
