#ifndef DB_H
#define DB_H

#include "../protocol.h"
#include "db_utils.h"
#include "config.h"

db_status_t db_insert_user(const user_protocol_t *user);
db_status_t db_find_user(user_data_t *user, const char *search_name);
db_status_t db_edit_user(int user_id, const char *new_user_name, const char *new_user_email);
db_status_t db_delete_user(int user_id);
db_status_t db_list_users(void);

#endif