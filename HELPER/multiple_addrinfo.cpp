#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct ServerSpec {
    std::string host;
    int port;
};

struct ResEntry {
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    sockaddr_storage addr;   // sockaddr_storage holds IPv4 or IPv6 while sockaddr holds only IPv4
    socklen_t addrlen;
    std::string canonname;   // may be empty
};

void print_sockaddr(const sockaddr *sa) {
    if (!sa) return;
    if (sa->sa_family == AF_INET) {
        const sockaddr_in *sin = reinterpret_cast<const sockaddr_in*>(sa);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
        std::cout << "  IP Address: " << ip << ", Port: " << ntohs(sin->sin_port) << "\n";
    } else if (sa->sa_family == AF_INET6) {
        const sockaddr_in6 *sin6 = reinterpret_cast<const sockaddr_in6*>(sa);
        char ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &sin6->sin6_addr, ip, sizeof(ip));
        std::cout << "  IP Address: " << ip << ", Port: " << ntohs(sin6->sin6_port) << "\n";
    } else {
        std::cout << "  Unknown family: " << sa->sa_family << "\n";
    }
}

int main() {
std::vector<ServerSpec> servers_to_resolve = {
    { "www.google.com", 8010 },
    { "www.wikipedia.org", 80 },
    { "www.github.com", 80 },
    // { "zakariaguellouch.com", 80 },
};


    // Hints: for resolving explicit hostnames, do NOT set AI_PASSIVE.
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      // AF_INET - AF_INET6 - AF_UNSPEC works on both protocols
    hints.ai_socktype = SOCK_STREAM;            // 0 for both SOCK_STREAM (TCP) and SOCK_DGRAM (UDP)
    hints.ai_flags = AI_CANONNAME;               // no AI_PASSIVE here (✔ You are a server ✔ You want to bind to your own machine ✔ You pass NULL for the host ✔ You are preparing to call socket → bind → listen) | the resulting address is intended for a server that will bind() and listen().

    std::vector<ResEntry> all_results;

    for (const auto &s : servers_to_resolve) {
        addrinfo *res = NULL;
        std::string port_str = std::to_string(s.port);
        int err = getaddrinfo(s.host.c_str(), port_str.c_str(), &hints, &res);
        if (err != 0) {
            std::cerr << "getaddrinfo failed for " << s.host << ":" << port_str
                      << " -> " << gai_strerror(err) << "\n";
            continue;
        }

        for (addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
            ResEntry entry;
            entry.ai_family = rp->ai_family;
            entry.ai_socktype = rp->ai_socktype;
            entry.ai_protocol = rp->ai_protocol;
            entry.addrlen = rp->ai_addrlen;
            std::memset(&entry.addr, 0, sizeof(entry.addr));
            if (rp->ai_addr && rp->ai_addrlen <= sizeof(entry.addr)) {
                std::memcpy(&entry.addr, rp->ai_addr, rp->ai_addrlen);
            }
            entry.canonname = rp->ai_canonname ? rp->ai_canonname : std::string();
            all_results.push_back(std::move(entry));
        }

        freeaddrinfo(res); // free each res after copying
    }

    // Print results
    for (const auto &r : all_results) {
        std::cout << "Result:\n";
        std::cout << "  Family: " << (r.ai_family == AF_INET ? "AF_INET" :
                                       (r.ai_family == AF_INET6 ? "AF_INET6" : "Other")) << "\n";
        std::cout << "  Socktype: " << (r.ai_socktype == SOCK_STREAM ? "SOCK_STREAM" :
                                         (r.ai_socktype == SOCK_DGRAM ? "SOCK_DGRAM" : "Other")) << "\n";
        std::cout << "  Protocol: " << r.ai_protocol << "\n";
        print_sockaddr(reinterpret_cast<const sockaddr*>(&r.addr));
        if (!r.canonname.empty()) {
            std::cout << "  Canonical name: " << r.canonname << "\n";
        } else {
            std::cout << "  No canonical name available\n";
        }
        std::cout << "------------------------------------\n";
    }

    return 0;
}
