#ifndef DB_H
#define DB_H

#include <stdint.h>
#include "../protocol.h"
#include "db_utils.h"
#include "config.h"

db_status_t db_insert_user(const user_protocol_t *user);
db_status_t db_find_user(user_data_t *out_users, int *out_count, const char *search_name, const char *last_name, int last_id);
db_status_t db_edit_user(uint32_t target_id, const char *new_user_name, const char *new_user_email, uint8_t new_access);
db_status_t db_delete_user(uint32_t target_id);

#endif