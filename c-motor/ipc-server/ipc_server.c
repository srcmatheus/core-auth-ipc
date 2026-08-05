#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc_server.h"

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