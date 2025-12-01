#include <iostream>
#include <cstring>
#include <netdb.h>      // getaddrinfo, addrinfo
#include <arpa/inet.h>  // inet_ntop
#include <sys/socket.h> // AF_INET, AF_INET6

void print_sockaddr(const sockaddr *sa) {
    char ipstr[INET6_ADDRSTRLEN];

    if (sa->sa_family == AF_INET) {
        sockaddr_in *sin = (sockaddr_in*)sa;
        inet_ntop(AF_INET, &(sin->sin_addr), ipstr, sizeof(ipstr));
        std::cout << "  IP: " << ipstr << " Port: " << ntohs(sin->sin_port) << "\n";
    } else if (sa->sa_family == AF_INET6) {
        sockaddr_in6 *sin6 = (sockaddr_in6*)sa;
        inet_ntop(AF_INET6, &(sin6->sin6_addr), ipstr, sizeof(ipstr));
        std::cout << "  IP: " << ipstr << " Port: " << ntohs(sin6->sin6_port) << "\n";
    } else {
        std::cout << "  Unknown AF\n";
    }
}

bool is_numeric_ip(const char *node) {
    sockaddr_in sa4;
    sockaddr_in6 sa6;
    return (inet_pton(AF_INET, node, &(sa4.sin_addr)) == 1) ||
           (inet_pton(AF_INET6, node, &(sa6.sin6_addr)) == 1);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <node> <service>\n";
        std::cerr << "Example: " << argv[0] << " www.example.com http\n";
        return 1;
    }

    const char *node = argv[1];
    const char *service = argv[2];

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // Both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if (is_numeric_ip(node)) {
        hints.ai_flags = AI_NUMERICHOST; // Node is an IP
    } else {
        hints.ai_flags = AI_CANONNAME;   // Node is a hostname
    }

    addrinfo *res = nullptr;
    int ret = getaddrinfo(node, service, &hints, &res);
    if (ret != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(ret) << "\n";
        return 1;
    }

    std::cout << "Results for " << node << " port/service " << service << ":\n";
    for (addrinfo *p = res; p != nullptr; p = p->ai_next) {
        std::cout << "  Family: " 
                  << (p->ai_family == AF_INET ? "AF_INET" : 
                      (p->ai_family == AF_INET6 ? "AF_INET6" : "Other")) << "\n";
        std::cout << "  Socktype: " 
                  << (p->ai_socktype == SOCK_STREAM ? "SOCK_STREAM" :
                      (p->ai_socktype == SOCK_DGRAM ? "SOCK_DGRAM" : "Other")) << "\n";
        std::cout << "  Protocol: " << p->ai_protocol << "\n";
        print_sockaddr(p->ai_addr);
        if (p->ai_canonname) {
            std::cout << "  Canonical name: " << p->ai_canonname << "\n";
        } else {
            std::cout << "  No canonical name available\n";
        }
        std::cout << "---------------------\n";
    }

    freeaddrinfo(res);
    return 0;
}
