#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t op_code;
    uint8_t access_level;
    char full_name[100];
    char email[100];
} user_protocol_t;

typedef struct {
    int id;
    char full_name[100];
    char email[100];
} user_data_t;

#endif