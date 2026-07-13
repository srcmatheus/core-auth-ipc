#ifndef DB_H
#define DB_H

#include "../protocol.h"
#include "config.h"

void db_init(db_config_t *config);
void db_close(void);

int db_insert_user(const user_protocol_t *user);

#endif