#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../database/config.h"

#define DEFAULT_SOCKET_PATH "/run/coreauth/coreauth.sock"
#define DEFAULT_MAX_CONN 10
#define DEFAULT_TIMEOUT_MS 6000

#define RX_BUFFER_SIZE 2048
#define TX_BUFFER_SIZE 32768

typedef struct {
    int32_t status;
    uint32_t count;
} ipc_response_header_t;

typedef struct{
    int fd;
    bool is_authenticated;
    
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t rx_bytes;

    uint8_t tx_buffer[TX_BUFFER_SIZE];
    size_t tx_bytes;
    size_t tx_offset;
} client_context_t;

typedef struct {
    const char *socket_path;
    const char *auth_token;
    int max_connections;
    int timeout_ms;
} ipc_config_t;

typedef enum {
    IPC_SUCCESS = 0,
    IPC_ERROR_SOCKET,
    IPC_ERROR_BIND,
    IPC_ERROR_LISTEN,
    IPC_ERROR_EPOLL,
    IPC_ERROR_DB
} ipc_status_t;

ipc_status_t ipc_server_start(const ipc_config_t *ipc_config, const db_config_t *db_config);

void ipc_server_stop(void);

#endif