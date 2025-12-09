#include <iostream>
#include <string>
#include <cstring>        // memset, strerror
#include <cerrno>         // errno
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>        // getaddrinfo, freeaddrinfo, addrinfo
#include <arpa/inet.h>    // inet_pton, htons, inet_ntop
#include <unistd.h>       // close, shutdown
#include <signal.h>       // signal, SIGPIPE

int running = 1;

int main(int ac, char **av) {
    if (ac != 2) {
        std::cerr << "Usage: " << av[0] << " <PORT_NUM>" << std::endl;
        return 1;
    }

    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE signals to avoid crashing when client disconnects

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(NULL, av[1], &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << "\n";
        return 1;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        freeaddrinfo(res);
        return 1;
    }

    if (listen(sock, 10) < 0) {
        perror("listen");
        freeaddrinfo(res);
        return 1;
    }

    std::cout << "Server listening on port " << av[1] << "...\n";

    while (running) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected...\n";

        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        // Receive data from the client
        ssize_t bytes_received = recv(client_sock, buffer, sizeof(buffer)-1, 0);
        if (bytes_received < 0) {
            perror("recv");
            close(client_sock);
            continue;
        }
        
        // If client disconnects or sends nothing, close the connection
        if (bytes_received == 0) {
            std::cout << "Client disconnected...\n";
            close(client_sock);
            continue;
        }

        std::cout << "Received: " << buffer << std::endl;

        // Send a response back to the client
        std::string response = "Message received: " + std::string(buffer);
        ssize_t bytes_sent = send(client_sock, response.c_str(), response.size(), 0);
        if (bytes_sent < 0) {
            perror("send");
        }

        // Close client socket after communication
        close(client_sock);
    }

    freeaddrinfo(res);
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}
