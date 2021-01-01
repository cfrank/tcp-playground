#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define HOST "127.0.0.1"
#define DEFAULT_PORT "4986"
#define MAX_BACKLOG 128
#define MAX_EVENTS 1024

static bool make_fd_non_blocking(int fd)
{
        int fl = fcntl(fd, F_GETFL, 0);

        if (fl == -1) {
                return false;
        }

        fl |= O_NONBLOCK;

        if (fcntl(fd, F_SETFL, fl) == -1) {
                return false;
        }

        return true;
}

/**
 * The goal of this small program is to accept an incoming connection
 * and return the string "Hello world" back to the peer
 */
int main(int argc, const char **argv)
{
        const char *port = DEFAULT_PORT;

        if (argc == 2) {
                port = argv[1];
        }

        // Set up address
        struct addrinfo hints;
        struct addrinfo *addrs = NULL;

        memset(&hints, 0, sizeof(struct addrinfo));

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;

        if (getaddrinfo(HOST, port, &hints, &addrs) != 0) {
                puts("Failed to call getaddrinfo");

                return EXIT_FAILURE;
        }

        if (addrs == NULL) {
                puts("Invalid addrs result");

                return EXIT_FAILURE;
        }

        int fd = -1;

        const struct addrinfo *addr = NULL;
        for (addr = addrs; addr != NULL; addr = addr->ai_next) {
                fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

                if (fd < 0) {
                        continue;
                }

                if (bind(fd, addr->ai_addr, addr->ai_addrlen) < 0) {
                        puts("Failed to bind fd");

                        close(fd);

                        freeaddrinfo(addrs);

                        return EXIT_FAILURE;
                }

                if (listen(fd, MAX_BACKLOG) != 0) {
                        close(fd);

                        freeaddrinfo(addrs);

                        puts("Failed to listen to fd");

                        return EXIT_FAILURE;
                }

                break;
        }

        freeaddrinfo(addrs);

        if (fd < 0) {
                puts("Invalid FD");

                return EXIT_FAILURE;
        }

        if (!make_fd_non_blocking(fd)) {
                freeaddrinfo(addrs);

                return EXIT_FAILURE;
        }

        int epoll_fd = epoll_create(1);

        if (epoll_fd == -1) {
                puts("Failed to get epoll fd");

                return EXIT_FAILURE;
        }

        // Add fd to epoll
        struct epoll_event ev = {};

        ev.events = EPOLLIN;
        ev.data.fd = fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                puts("Failed to add fd to epoll");

                return EXIT_FAILURE;
        }

        struct epoll_event events[MAX_EVENTS];
        int ready_fds = -1;

        for (;;) {
                ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

                if (ready_fds == -1) {
                        puts("epoll_wait failed");

                        return EXIT_FAILURE;
                }

                for (int i = 0; i < ready_fds; ++i) {
                        if (events[i].data.fd == fd) {
                                printf("Got %d ready fds", ready_fds);
                        }

                        return EXIT_SUCCESS;
                }
        }

        return EXIT_SUCCESS;
}
