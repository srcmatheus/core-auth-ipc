#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "../protocol.h"
#include "db_utils.h"


db_status_t db_insert_user(const user_protocol_t *user){

    if (user == NULL) {
        return DB_WARNING; 
    }

    unsigned long len_name = strlen(user->full_name);
    unsigned long len_email = strlen(user->email);

    if (len_name > 100 || len_email > 100 || len_name == 0 || len_email == 0) {
        return DB_WARNING;
    }

    MYSQL_BIND bind[3] = {0};

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)user->full_name;
    bind[0].buffer_length = len_name;
    bind[0].length = &len_name;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)user->email;
    bind[1].buffer_length = len_email;
    bind[1].length = &len_email;

    bind[2].buffer_type = MYSQL_TYPE_TINY;
    bind[2].buffer = (void *)&user->target_level;
    bind[2].buffer_length = sizeof(user->target_level);

    return db_execute_stmt(db_stmts.insert_user, bind);
}

db_status_t db_find_user(user_data_t *out_users, int *out_count, const char *search_name, const char *last_name, int last_id) {

    if(out_users == NULL || out_count == NULL) {
        return DB_WARNING;
    }

    *out_count = 0;

    char search_buffer[105] = {0};
    if(search_name == NULL || search_name[0] == '\0') {
        strcpy(search_buffer, "%");
    } else {
        snprintf(search_buffer, sizeof(search_buffer), "%%%s%%", search_name);
    }

    unsigned long len_search = strlen(search_buffer);

    const char *cursor_name = (last_name != NULL) ? last_name : "";
    unsigned long len_cursor_name = strlen(cursor_name);
    int cursor_id = (last_id > 0) ? last_id : 0;

    MYSQL_BIND bind_in[4] = {0};

    bind_in[0].buffer_type = MYSQL_TYPE_STRING;
    bind_in[0].buffer = search_buffer;
    bind_in[0].buffer_length = len_search;
    bind_in[0].length = &len_search;

    bind_in[1].buffer_type = MYSQL_TYPE_STRING;
    bind_in[1].buffer = (char *)cursor_name;
    bind_in[1].buffer_length = len_cursor_name;
    bind_in[1].length = &len_cursor_name;

    bind_in[2].buffer_type = MYSQL_TYPE_STRING;
    bind_in[2].buffer = (char *)cursor_name;
    bind_in[2].buffer_length = len_cursor_name;
    bind_in[2].length = &len_cursor_name;

    bind_in[3].buffer_type = MYSQL_TYPE_LONG;
    bind_in[3].buffer = &cursor_id;
    bind_in[3].buffer_length = sizeof(cursor_id);

    if(db_execute_stmt(db_stmts.find_user, bind_in) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    int out_id = 0;
    char out_name[100] = {0};
    char out_email[100] = {0};
    uint8_t out_access = 0;
    unsigned long len_out_name = 0;
    unsigned long len_out_email = 0;

    MYSQL_BIND bind_out[4] = {0};

    bind_out[0].buffer_type = MYSQL_TYPE_LONG;
    bind_out[0].buffer = &out_id;
    bind_out[0].buffer_length = sizeof(out_id);

    bind_out[1].buffer_type = MYSQL_TYPE_STRING;
    bind_out[1].buffer = out_name;
    bind_out[1].buffer_length = sizeof(out_name);
    bind_out[1].length = &len_out_name;

    bind_out[2].buffer_type = MYSQL_TYPE_STRING;
    bind_out[2].buffer = out_email;
    bind_out[2].buffer_length = sizeof(out_email);
    bind_out[2].length = &len_out_email;

    bind_out[3].buffer_type = MYSQL_TYPE_TINY;
    bind_out[3].buffer = &out_access;
    bind_out[3].buffer_length = sizeof(out_access);

    if(mysql_stmt_bind_result(db_stmts.find_user, bind_out)) {
        fprintf(stderr, "Error binding results: %s\n", mysql_stmt_error(db_stmts.find_user));
        return DB_CRITICAL_ERROR;
    }

    if(mysql_stmt_store_result(db_stmts.find_user)) {
        fprintf(stderr, "Error storing results: %s\n", mysql_stmt_error(db_stmts.find_user));
        return DB_CRITICAL_ERROR;
    }

    int fetch_res;
    while((fetch_res = mysql_stmt_fetch(db_stmts.find_user)) == 0 || fetch_res == MYSQL_DATA_TRUNCATED) {
        
        if(*out_count >= 50) break;

        out_name[len_out_name < sizeof(out_name) ? len_out_name : sizeof(out_name) - 1] = '\0';
        out_email[len_out_email < sizeof(out_email) ? len_out_email : sizeof(out_email) - 1] = '\0';

        out_users[*out_count].id = out_id;
        strncpy(out_users[*out_count].full_name, out_name, sizeof(out_users[*out_count].full_name) - 1);
        strncpy(out_users[*out_count].email, out_email, sizeof(out_users[*out_count].email) - 1);

        out_users[*out_count].full_name[sizeof(out_users[*out_count].full_name) - 1] = '\0';
        out_users[*out_count].email[sizeof(out_users[*out_count].email) - 1] = '\0';
        out_users[*out_count].access_level = out_access;

        (*out_count)++;
    }

    mysql_stmt_free_result(db_stmts.find_user);

    if(fetch_res != MYSQL_NO_DATA && fetch_res != 0 && fetch_res != MYSQL_DATA_TRUNCATED) {
        fprintf(stderr, "Data fetch failure: %s\n", mysql_stmt_error(db_stmts.find_user));
        return DB_CRITICAL_ERROR;
    }

    if(*out_count == 0) {
        return DB_WARNING; 
    }

    return DB_SUCCESS;
}

db_status_t db_edit_user(uint32_t target_id, const char *new_user_name, const char *new_user_email, uint8_t new_access){

    if (target_id <= 0 || new_user_name == NULL || new_user_email == NULL) {
        return DB_WARNING;
    }

    unsigned long len_name = strlen(new_user_name);
    unsigned long len_email = strlen(new_user_email);

    MYSQL_BIND bind[4] = {0};

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)new_user_name;
    bind[0].buffer_length = len_name;
    bind[0].length = &len_name;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)new_user_email;
    bind[1].buffer_length = len_email;
    bind[1].length = &len_email;

    bind[2].buffer_type = MYSQL_TYPE_TINY;
    bind[2].buffer = (void *)&new_access;
    bind[2].buffer_length = sizeof(new_access);

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = (void *)&target_id;
    bind[3].buffer_length = sizeof(target_id);

    if(db_execute_stmt(db_stmts.edit_user, bind) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    my_ulonglong altered_rows = mysql_stmt_affected_rows(db_stmts.edit_user);

    if(altered_rows == (my_ulonglong)-1) return DB_CRITICAL_ERROR;

    return DB_SUCCESS;
}

db_status_t db_delete_user(uint32_t target_id){
    
    if(target_id == 0) return DB_WARNING;

    MYSQL_BIND bind[1] = {0};

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &target_id;
    bind[0].buffer_length = sizeof(target_id);

    if(db_execute_stmt(db_stmts.delete_user, bind) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    my_ulonglong altered_rows = mysql_stmt_affected_rows(db_stmts.delete_user);

    if(altered_rows == 0) return DB_WARNING;

    return DB_SUCCESS;
}