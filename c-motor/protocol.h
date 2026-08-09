#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t op_code;
    uint8_t access_level;
    uint32_t id;
    char full_name[100];
    char email[100];
    uint8_t target_level;
} user_protocol_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    char full_name[100];
    char email[100];
    uint8_t access_level;
} user_data_t;

typedef enum{
    OP_INSERT = 1,
    OP_FIND = 2,
    OP_EDIT = 3,
    OP_DELETE = 4
} ipc_op_code_t;

#endif