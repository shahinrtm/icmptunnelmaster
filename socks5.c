#include "socks5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUF_SIZE 4096

static void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    unsigned char buf[262];
    int n;

    // handshake
    n = read(client_fd, buf, 2);
    if (n != 2 || buf[0] != 5) {
        close(client_fd);
        return NULL;
    }
    n = read(client_fd, buf, buf[1]);
    if (n != buf[1]) {
        close(client_fd);
        return NULL;
    }
    unsigned char resp[2] = {5, 0};
    write(client_fd, resp, 2);

    // request
    n = read(client_fd, buf, 4);
    if (n != 4 || buf[1] != 1) {
        close(client_fd);
        return NULL;
    }
    unsigned char atyp = buf[3];
    if (atyp != 1) { // only IPv4
        close(client_fd);
        return NULL;
    }
    unsigned char addrbuf[4];
    n = read(client_fd, addrbuf, 4);
    if (n != 4) {
        close(client_fd);
        return NULL;
    }
    unsigned char portbuf[2];
    n = read(client_fd, portbuf, 2);
    if (n != 2) {
        close(client_fd);
        return NULL;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    memcpy(&remote_addr.sin_addr, addrbuf, 4);
    memcpy(&remote_addr.sin_port, portbuf, 2);

    int remote_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        close(client_fd);
        return NULL;
    }
    if (connect(remote_fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        close(remote_fd);
        close(client_fd);
        return NULL;
    }

    unsigned char rep[10] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    write(client_fd, rep, 10);

    fd_set fds;
    int maxfd = client_fd > remote_fd ? client_fd : remote_fd;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(remote_fd, &fds);
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) <= 0)
            break;
        if (FD_ISSET(client_fd, &fds)) {
            n = read(client_fd, buf, BUF_SIZE);
            if (n <= 0)
                break;
            write(remote_fd, buf, n);
        }
        if (FD_ISSET(remote_fd, &fds)) {
            n = read(remote_fd, buf, BUF_SIZE);
            if (n <= 0)
                break;
            write(client_fd, buf, n);
        }
    }

    close(remote_fd);
    close(client_fd);
    return NULL;
}

void run_socks5_server(int port)
{
    int srv_fd;
    struct sockaddr_in addr;

    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv_fd);
        return;
    }

    if (listen(srv_fd, 5) < 0) {
        perror("listen");
        close(srv_fd);
        return;
    }

    while (1) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd)
            break;
        *client_fd = accept(srv_fd, NULL, NULL);
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) == 0)
            pthread_detach(tid);
        else
            close(*client_fd);
    }

    close(srv_fd);
}

