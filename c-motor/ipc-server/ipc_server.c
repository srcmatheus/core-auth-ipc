#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "ipc_server.h"
#include "../database/db_utils.h"

#define MAX_EVENTS 64

static volatile sig_atomic_t keep_running = 1;

static void sig_handler(int signum){
    (void)signum;
    keep_running = 0;
}

static void setup_signals(void){
    struct sigaction sa = {0};
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void ipc_server_stop(void) {
    keep_running = 0;
}

static int set_nonblocking(int fd){

    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        perror("fcntl F_GETFL failed");
        return -1;
    }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        perror("fcntl F_SETFL O_NONBLOCK failed");
        return -1;
    }

    return 0;
}

static int bind_socket(const char *socket_path, int max_connections){

    int server_fd;
    struct sockaddr_un addr = {0};

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(server_fd == -1){
        perror("socket creation failed");
        return -1;
    }

    unlink(socket_path);

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1){
        perror("socket bind failed");
        close(server_fd);
        return -1;
    }

    if(set_nonblocking(server_fd) < 0){
        close(server_fd);
        unlink(socket_path);
        return -1;
    }

    if(listen(server_fd, max_connections) == -1){
        perror("socket listen failed");
        close(server_fd);
        unlink(socket_path);
        return -1;
    }

    return server_fd;
}

static client_context_t* client_context(int fd){

    if(fd < 0){
        return NULL;
    }

    client_context_t *ctx = (client_context_t *)malloc(sizeof(client_context_t));
    if(ctx == NULL){
        perror("malloc failed for client_context_t");
        return NULL;
    }

    memset(ctx, 0, sizeof(client_context_t));

    ctx->fd = fd;

    return ctx;
}

static void client_destroy(client_context_t *ctx){

    if(ctx == NULL){
        return;
    }

    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    free(ctx);
}

static void handle_accept(int epoll_fd, int server_fd){

    while(1){
        
        int client_fd = accept4(server_fd, NULL, NULL, SOCK_NONBLOCK);

        if(client_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;

            if(errno == EINTR) continue;

            perror("accept failed");
            break;
        }

        client_context_t *ctx = client_context(client_fd);
        if(ctx == NULL){
            close(client_fd);
            continue;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.ptr = ctx;

        if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0){
            perror("epoll_ctl: client_fd failed");
            client_destroy(ctx);
            continue;
        }
    }
}

static void handle_client_write(int epoll_fd, client_context_t *ctx) {

    if(ctx == NULL || ctx->fd < 0 || ctx->tx_bytes == 0) return;

    while(ctx->tx_offset < ctx->tx_bytes){
        ssize_t bytes_written = write(ctx->fd, 
                                      ctx->tx_buffer + ctx->tx_offset, 
                                      ctx->tx_bytes - ctx->tx_offset);

        if(bytes_written < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            if(errno == EINTR) continue;

            perror("write failed");
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL);
            client_destroy(ctx);
            return;
        }

        ctx->tx_offset += (size_t)bytes_written;
    }

    if(ctx->tx_offset == ctx->tx_bytes){
        ctx->tx_bytes = 0;
        ctx->tx_offset = 0;
    }
}

static void handle_client_data(int epoll_fd, client_context_t *ctx){

    while(1){
        size_t free_space = RX_BUFFER_SIZE - ctx->rx_bytes;

        if(free_space == 0){
            fprintf(stderr, "Warning: rx_buffer full on fd %d. Closing connection.\n", ctx->fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL);
            client_destroy(ctx);
            return;
        }

        ssize_t bytes_read = read(ctx->fd, ctx->rx_buffer + ctx->rx_bytes, free_space);

        if(bytes_read < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            if(errno == EINTR) continue;

            perror("read failed");
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL);
            client_destroy(ctx);
            return;
        }

        if (bytes_read == 0) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL);
            client_destroy(ctx);
            return;
        }

        //Handshake token

        ctx->rx_bytes += (size_t)bytes_read;
    }

    parse_and_execute_command(epoll_fd, ctx);
}

ipc_status_t ipc_server_start(const ipc_config_t *ipc_config, const db_config_t *db_config){

    setup_signals();

    if(ipc_config == NULL || db_config == NULL) return IPC_ERROR_SOCKET;

    const char *socket_path = (ipc_config->socket_path != NULL) ? ipc_config->socket_path : DEFAULT_SOCKET_PATH;
    int max_conn = (ipc_config->max_connections > 0) ? ipc_config->max_connections : DEFAULT_MAX_CONN;
    //const char *auth_token = (ipc_config->auth_token != NULL) ? ipc_config->auth_token : DEFAULT_AUTH_TOKEN;
    int timeout = (ipc_config->timeout_ms > 0) ? ipc_config->timeout_ms : DEFAULT_TIMEOUT_MS;

    int server_fd = bind_socket(socket_path, max_conn);
    if(server_fd < 0){
        return IPC_ERROR_BIND;
    }

    int epoll_fd = epoll_create1(0);
    if(epoll_fd < 0){
        perror("epoll_create1 failed");

        close(server_fd);
        unlink(socket_path);
        server_fd = -1;

        return IPC_ERROR_EPOLL;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0){
        perror("epoll_ctl: server_fd failed");

        close(epoll_fd);
        close(server_fd);
        unlink(socket_path);

        epoll_fd = -1;
        server_fd = -1;

        return IPC_ERROR_EPOLL;
    }

    struct epoll_event events[MAX_EVENTS];

    while(keep_running){

        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);

        if(num_events < 0){
            if(errno == EINTR) continue;

            perror("epoll_wait failed");
            break;
        }

        if(num_events == 0){
            db_ping(db_config);
            continue;
        }

        for(int i = 0; i < num_events; i++){
            if(events[i].data.fd == server_fd){
                handle_accept(epoll_fd, server_fd);
            }else{
                client_context_t *ctx = (client_context_t *)events[i].data.ptr;
                handle_client_data(epoll_fd, ctx);
            }
        }
    }

    if(epoll_fd >= 0){
        close(epoll_fd);
        epoll_fd = -1;
    }

    if(server_fd >= 0){
        close(server_fd);
        server_fd = -1;
    }

    if(socket_path){
        unlink(socket_path);
        socket_path = NULL;
    }

    return IPC_SUCCESS;
}