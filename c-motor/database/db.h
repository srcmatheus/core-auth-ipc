#ifndef DB_H
#define DB_H

#include "../protocol.h"
#include "config.h"

void db_init(db_config_t *config);
void db_close(void);

int db_insert_user(const user_protocol_t *user);

int db_find_user(user_data_t *user, char *search_name);

#endif