#ifndef DB_H
#define DB_H

#include "../protocol.h"
#include "config.h"

void db_init(db_config_t *config);
void db_close(void);

int db_insert_user(const user_protocol_t *user);

int db_find_user(user_data_t *user, const char *search_name);

int db_edit_user(int user_id, const char *new_user_name, const char *new_user_email);

int db_delete_user(int user_id);

int db_list_users(void);

#endif