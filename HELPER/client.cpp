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


static ssize_t send_all(int sockfd, const char* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(sent);
}

int main(int ac, char **av) {
    if (ac != 3) {
        std::cerr << "Usage: " <<  av[0] << " <HOST||IP> <PORT_NUM>" << std::endl;
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(av[1], av[2], &hints, &res);
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

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    std::string request = "";
        // "GET / HTTP/1.1\r\n"
        // "Host: " + std::string(av[1]) + "\r\n"
        // "User-Agent: cpp-simple-client\r\n"
        // "Connection: close\r\n"
        // "\r\n";

    if (send_all(sock, request.c_str(), request.size()) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    std::string response;
    char buf[4096];
    while (true) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n > 0) {
            response.append(buf, n);
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }
    }

    std::cout << "----- response start -----\n"
              << response
              << "\n----- response end -----\n";

    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}

