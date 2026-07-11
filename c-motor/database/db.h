#ifndef DB_H
#define DB_H

#include "protocol.h"

void db_init(void);
void db_close(void);

int db_insert_user(const user_protocol_t *user);

#endif