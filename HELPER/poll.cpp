#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define PORT "9030"   /* Port we're listening on */
#define INITIAL_FD_SIZE 5
#define MAX_BACKLOG 10

/* Convert sockaddr_storage (AF_INET/AF_INET6) into printable IP string.
 * Returns pointer to buf on success or NULL on unsupported family.
 */
const char* inet_ntop2(void* addr, char* buf, size_t size) {
    struct sockaddr_storage* sas = (struct sockaddr_storage*)addr;

    if (sas->ss_family == AF_INET) {
        struct sockaddr_in* sa4 = (struct sockaddr_in*)addr;
        return inet_ntop(AF_INET, &sa4->sin_addr, buf, size);
    } else if (sas->ss_family == AF_INET6) {
        struct sockaddr_in6* sa6 = (struct sockaddr_in6*)addr;
        return inet_ntop(AF_INET6, &sa6->sin6_addr, buf, size);
    } else {
        return nullptr;
    }
}

/* Create, bind and listen on a socket. Returns listening fd or -1 on error. */
int get_listener_socket(void) {
    int listener = -1;
    int yes = 1;
    int rv;
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP (INADDR_ANY) */

    if ((rv = getaddrinfo(nullptr, PORT, &hints, &ai)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }

    /* Try each returned addr until we bind successfully. */
    for (p = ai; p != nullptr; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener == -1) {
            continue;
        }

        /* Avoid "address already in use" on quick restarts */
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            close(listener);
            listener = -1;
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            listener = -1;
            continue;
        }

        /* success */
        break;
    }

    freeaddrinfo(ai);

    if (p == nullptr || listener == -1) {
        std::cerr << "Failed to bind listener socket" << std::endl;
        return -1;
    }

    if (listen(listener, MAX_BACKLOG) == -1) {
        perror("listen");
        close(listener);
        return -1;
    }

    return listener;
}

/* Append newfd to a (resizable) array of struct pollfd. */
void add_to_pfds(std::vector<struct pollfd>& pfds, int newfd) {
    struct pollfd pfd;
    pfd.fd = newfd;
    pfd.events = POLLIN;   /* Ask poll() to tell us when data is readable */
    pfd.revents = 0;
    pfds.push_back(pfd);
}

/* Remove the entry at index i by copying the last entry over it. */
void del_from_pfds(std::vector<struct pollfd>& pfds, int i) {
    pfds[i] = pfds[pfds.size() - 1];
    pfds.pop_back();
}

/* Accept a new client, add to pfds. */
void handle_new_connection(int listener, std::vector<struct pollfd>& pfds) {
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen = sizeof(remoteaddr);
    int newfd;
    char remoteIP[INET6_ADDRSTRLEN];

    newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);
    if (newfd == -1) {
        perror("accept");
        return;
    }

    add_to_pfds(pfds, newfd);

    const char* ip = inet_ntop2(&remoteaddr, remoteIP, sizeof(remoteIP));
    if (ip)
        std::cout << "pollserver: new connection from " << ip << " on socket " << newfd << std::endl;
    else
        std::cout << "pollserver: new connection on socket " << newfd << " (unknown family)" << std::endl;
}

/* Receive data from a client and broadcast to others. */
void handle_client_data(int listener, std::vector<struct pollfd>& pfds, int i) {
    char buf[1024];
    ssize_t nbytes;
    int sender_fd = pfds[i].fd;

    nbytes = recv(sender_fd, buf, sizeof(buf), 0);

    if (nbytes <= 0) {
        if (nbytes == 0) {
            /* Connection closed by client */
            std::cout << "pollserver: socket " << sender_fd << " hung up" << std::endl;
        } else {
            perror("recv");
        }

        close(sender_fd);
        del_from_pfds(pfds, i);
    } else {
        /* Print received bytes */
        std::cout << "pollserver: recv from fd " << sender_fd << " (" << nbytes << " bytes): |" 
                  << std::string(buf, nbytes) << "|" << std::endl;

        /* Broadcast to all other clients (not listener, not sender) */
        for (size_t j = 0; j < pfds.size(); j++) {
            int dest_fd = pfds[j].fd;
            if (dest_fd == listener || dest_fd == sender_fd)
                continue;

            ssize_t sent = send(dest_fd, buf, nbytes, 0);
            if (sent == -1) {
                perror("send");
            }
        }
    }
}

/* Iterate pfds and handle sockets that have revents set by poll(). */
void process_connections(int listener, std::vector<struct pollfd>& pfds) {
    for (size_t i = 0; i < pfds.size(); i++) {
        short rev = pfds[i].revents;

        if (rev == 0) {
            continue; /* nothing happened on this fd */
        }

        /* Handle fatal conditions first: errors, invalid fds, hangups */
        if (rev & (POLLERR | POLLNVAL)) {
            std::cerr << "pollserver: fd " << pfds[i].fd << " has error or invalid (revents=0x" 
                      << std::hex << rev << "). Closing." << std::endl;
            close(pfds[i].fd);
            del_from_pfds(pfds, i);
            i--; /* re-check the slot that was swapped in */
            continue;
        }

        if (rev & POLLHUP) {
            /* Peer closed connection (hangup) */
            std::cout << "pollserver: fd " << pfds[i].fd << " hangup (revents=0x" << std::hex << rev 
                      << "). Closing." << std::endl;
            close(pfds[i].fd);
            del_from_pfds(pfds, i);
            i--;
            continue;
        }

        /* The normal readable case */
        if (rev & POLLIN) {
            if (pfds[i].fd == listener) {
                /* New incoming connection ready to accept */
                handle_new_connection(listener, pfds);
            } else {
                /* Data available to read from a client */
                handle_client_data(listener, pfds, i);
            }
        }

        /* Reset revents for next poll loop (not required, poll overwrites on next call) */
        pfds[i].revents = 0;
    }
}

int main(void) {
    int listener = get_listener_socket();
    if (listener == -1) {
        std::cerr << "error getting listening socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    /* Start pfds with some initial space */
    std::vector<struct pollfd> pfds;
    pfds.push_back({listener, POLLIN, 0});

    std::cout << "pollserver: waiting for connections on port " << PORT << std::endl;

    /* Main loop */
    for (;;) {
        int ready = poll(pfds.data(), pfds.size(), -1); /* -1 => block indefinitely */
        if (ready == -1) {
            if (errno == EINTR) {
                /* Interrupted by signal; continue looping */
                continue;
            } else {
                perror("poll");
                break;
            }
        }

        if (ready == 0) {
            /* timed out — won't happen here because timeout = -1 */
            continue;
        }

        /* Process the fds that have events set in revents */
        process_connections(listener, pfds);
    }

    /* Clean up */
    for (size_t i = 0; i < pfds.size(); i++) {
        close(pfds[i].fd);
    }
    close(listener);
    return 0;
}
