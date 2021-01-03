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
#define READ_SIZE 1000
#define MESSAGE "Hello world"

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

static struct addrinfo *get_addrinfo(const char *port)
{
        struct addrinfo *res = NULL;
        struct addrinfo hints;

        memset(&hints, 0, sizeof(struct addrinfo));

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;

        if (getaddrinfo(HOST, port, &hints, &res) != 0) {
                puts("Failed to call getaddrinfo");

                return NULL;
        }

        return res;
}

static int get_listener(const struct addrinfo *addrinfo)
{
        int fd = -1;
        const struct addrinfo *addr = NULL;

        for (addr = addrinfo; addr != NULL; addr = addr->ai_next) {
                fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

                if (fd < 0) {
                        continue;
                }

                if (bind(fd, addr->ai_addr, addr->ai_addrlen) < 0) {
                        puts("Failed to bind fd");

                        close(fd);

                        return -1;
                }

                if (listen(fd, MAX_BACKLOG) < 0) {
                        puts("Failed to listen to fd");

                        close(fd);

                        return -1;
                }

                break;
        }

        return fd;
}

/**
 * Accept a connection and return "Hello world"
 */
int main(int argc, const char **argv)
{
        const char *port = DEFAULT_PORT;

        if (argc == 2) {
                port = argv[1];
        }

        struct addrinfo *addrinfo = get_addrinfo(port);

        if (addrinfo == NULL) {
                return EXIT_FAILURE;
        }

        int listen_fd = get_listener(addrinfo);

        freeaddrinfo(addrinfo);

        if (listen_fd == -1) {
                return EXIT_FAILURE;
        }

        if (!make_fd_non_blocking(listen_fd)) {
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
        ev.data.fd = listen_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
                puts("Failed to add listen_fd to epoll");

                goto handle_error;
        }

        struct epoll_event events[MAX_EVENTS];
        int ready_fds = -1;

        for (;;) {
                ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

                if (ready_fds == -1) {
                        goto handle_error;
                }

                for (int i = 0; i < ready_fds; ++i) {
                        if (events[i].events & EPOLLERR) {
                                puts("epoll error");

                                goto handle_error;
                        }
                        if (events[i].data.fd == listen_fd) {
                                // Handle new peer
                                struct sockaddr_in peer_addr;
                                socklen_t peer_addr_len = sizeof(peer_addr);
                                int new_peer_fd = accept(
                                        listen_fd, (struct sockaddr *)&peer_addr, &peer_addr_len);

                                if (new_peer_fd < 0) {
                                        puts("Failed to accept");

                                        goto handle_error;
                                }

                                if (!make_fd_non_blocking(new_peer_fd)) {
                                        puts("Failed to make peer fd non blocking");

                                        close(new_peer_fd);

                                        goto handle_error;
                                }

                                if (new_peer_fd >= MAX_EVENTS) {
                                        puts("Too many events");

                                        close(new_peer_fd);

                                        goto handle_error;
                                }

                                struct epoll_event new_peer_event = {0};
                                new_peer_event.data.fd = new_peer_fd;
                                // we will only be writing to this fd
                                new_peer_event.events |= EPOLLOUT;

                                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_peer_fd, &new_peer_event)
                                    < 0) {
                                        puts("Failed to add new peer fd to epoll");

                                        close(new_peer_fd);

                                        goto handle_error;
                                }
                        } else if (events[i].events & EPOLLOUT) {
                                // Write to this FD
                                int message_len = sizeof(MESSAGE);
                                int sent_cnt = send(events[i].data.fd, MESSAGE, message_len, 0);

                                if (sent_cnt < 0) {
                                        puts("Failed to send message");

                                        goto handle_error;
                                }

                                puts("disconnecting");

                                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL)
                                    < 0) {
                                        puts("Failed to delete fd from epoll");

                                        close(events[i].data.fd);

                                        goto handle_error;
                                }
                        }
                }
        }

        return EXIT_SUCCESS;

handle_error:
        if (close(epoll_fd) != 0) {
                puts("Failed to close epoll fd");
        }

        return EXIT_FAILURE;
}
