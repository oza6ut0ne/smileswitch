#include <iostream>
#include <queue>
#include <sstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "util.hpp"

const char *const LISTEN_PORT = "2525";
const int NUM_CLIENTS = 5;
const unsigned int CHUNK_SIZE = 1024;


std::string getaddrstr(sockaddr_storage sockaddr) {
    const char *res = NULL;
    char buf[INET6_ADDRSTRLEN];
    memset(buf, 0, sizeof(buf));

    if (sockaddr.ss_family == AF_INET) {
        const sockaddr_in *addr = (sockaddr_in *) &sockaddr;
        res = inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
    } else if (sockaddr.ss_family == AF_INET6) {
        const sockaddr_in6 *addr = (sockaddr_in6 *) &sockaddr;
        res = inet_ntop(AF_INET6, &addr->sin6_addr, buf, sizeof(buf));
    }

    if (res == NULL) {
        res = "NULL";
    }
    return std::string(res);
}

int getaddrinfo_v6first(const char *service, addrinfo **result) {
    addrinfo hints;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = IPPROTO_TCP;

    int gai_errno;
    gai_errno = getaddrinfo(NULL, service, &hints, result);
    if (gai_errno != 0) {
        hints.ai_family = AF_INET;
        gai_errno = getaddrinfo(NULL, service, &hints, result);
    }
    return gai_errno;
}

void reset_client(int *fd, std::stringstream *ss) {
    ss->str("");
    ss->clear();
    close(*fd);
    *fd = -1;
}

int tcp_listen(const char *service) {
    int sfd;
    addrinfo *ai;

    int gai_errno;
    gai_errno = getaddrinfo_v6first(service, &ai);
    if (gai_errno != 0) {
        std::cerr << "getaddrinfo() failed" << std::endl;
        std::cerr << gai_strerror(gai_errno) << std::endl;
        return -1;
    }

    if ((sfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0) {
        handle_error("socket() failed: fd = " + std::to_string(sfd));
        close(sfd);
        return -1;
    }

    const int ov_reuseaddr = 1;
    if ((setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &ov_reuseaddr, sizeof(ov_reuseaddr))) < 0) {
        handle_error("setsockopt() failed");
    }

    if (bind(sfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        handle_error("bind() failed");
        close(sfd);
        return -1;
    }

    if (listen(sfd, 1) < 0) {
        handle_error("listen() failed");
        close(sfd);
        return -1;
    }

    return sfd;
}

void *serve(void *arg) {
    std::queue<std::string> *const comment_queue = (std::queue<std::string> *) arg;

    const int sfd = tcp_listen(LISTEN_PORT);
    if (sfd < 0) {
        return NULL;
    }

    int cfds[NUM_CLIENTS];
    std::stringstream ss[NUM_CLIENTS];
    timespec ts[NUM_CLIENTS];
    memset(cfds, -1, sizeof(cfds));

    fd_set rfds;
    const double recv_timeout = 5;

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(sfd, &rfds);

        int maxfd = sfd;
        for (int fd : cfds) {
            if (fd >= 0) {
                FD_SET(fd, &rfds);
                maxfd = (maxfd < fd) ? fd : maxfd;
            }
        }

        timeval select_timeout;
        select_timeout.tv_sec = 1;
        select_timeout.tv_usec = 0;
        const int ready = select(maxfd + 1, &rfds, NULL, NULL, &select_timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            handle_error("select() failed: ");
            close(sfd);
            break;
        }

        if (FD_ISSET(sfd, &rfds)) {
            for (int i = 0; i < NUM_CLIENTS; i++) {
                if (cfds[i] >=0) {
                    continue;
                }

                sockaddr_storage client_addr;
                socklen_t client_addr_size = sizeof(sockaddr_storage);
                if ((cfds[i] = accept(sfd, (sockaddr *) &client_addr, &client_addr_size)) < 0) {
                    handle_error("accept() failed");
                    close(cfds[i]);
                    break;
                }

                std::cout << "connected from " << getaddrstr(client_addr) << std::endl;
                clock_gettime(CLOCK_REALTIME, &ts[i]);
                break;
            }
        }

        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (cfds[i] < 0) {
                continue;
            }
            if (!FD_ISSET(cfds[i], &rfds)) {
                timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                const double diff = (now.tv_sec - ts[i].tv_sec) + 1e-9 * (now.tv_nsec - ts[i].tv_nsec);
                if (diff > recv_timeout) {
                    // std::cout << ss[i].str() << std::endl;
                    comment_queue->push(ss[i].str());
                    reset_client(&cfds[i], &ss[i]);
                }
                continue;
            }
            // std::cout << "cfds[" << i << "] = " << cfds[i] << std::endl;

            char chunk[CHUNK_SIZE];
            const ssize_t recv_size = recv(cfds[i], chunk, sizeof(chunk), MSG_DONTWAIT);
            // std::cout << "recv_size: " << recv_size << std::endl;
            if (recv_size < 0) {
                handle_error("recv() failed");
                reset_client(&cfds[i], &ss[i]);
                continue;
            }

            ss[i] << std::string(chunk, recv_size);
            if (recv_size < CHUNK_SIZE) {
                // std::cout << ss[i].str() << std::endl;
                comment_queue->push(ss[i].str());
                reset_client(&cfds[i], &ss[i]);
                continue;
            }
        }
    }
    close(sfd);
    return NULL;
}

#ifndef __SWITCH__
int main(int argc, char** argv) {
    pthread_t thread;
    std::queue<std::string> comment_queue;

    const int err = pthread_create(&thread, NULL, serve, &comment_queue);
    if (err != 0) {
        std::cerr << "pthread_create() faild" << std::endl;
        return -1;
    }
    pthread_detach(thread);

    while(1) {
        timeval select_timeout;
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = 100000;
        select(0, NULL, NULL, NULL, &select_timeout);

        if (comment_queue.empty()) {
            continue;
        }
        std::cout << comment_queue.front() << std::endl;
        comment_queue.pop();
    }
}
#endif
