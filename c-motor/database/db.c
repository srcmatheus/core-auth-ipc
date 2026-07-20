#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "../protocol.h"
#include "db_utils.h"


db_status_t db_insert_user(const user_protocol_t *user){

    MYSQL_STMT *stmt = NULL;

    if(db_stmt_init(&stmt) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    const char query[] = "INSERT INTO users(name, email) VALUES(?, ?)";
    unsigned long length = strlen(query);

    unsigned long len_name = strlen(user->full_name);
    unsigned long len_email = strlen(user->email);

    MYSQL_BIND bind[2];

    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)user->full_name;
    bind[0].buffer_length = sizeof(user->full_name);
    bind[0].length = &len_name;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)user->email;
    bind[1].buffer_length = sizeof(user->email);
    bind[1].length = &len_email;

    if(db_stmt_set(&stmt, query, length, bind) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    mysql_stmt_close(stmt);

    return DB_SUCCESS;
}

db_status_t db_find_user(user_data_t *user, const char *search_name){

    MYSQL_STMT *stmt = NULL;

    if(db_stmt_init(&stmt) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    const char query[] = "SELECT id, name, email FROM users WHERE name = ?";
    unsigned long length = strlen(query);
    unsigned long len_search_name = strlen(search_name);

    MYSQL_BIND bind_query[1];
    memset(bind_query, 0, sizeof(bind_query));

    bind_query[0].buffer_type = MYSQL_TYPE_STRING;
    bind_query[0].buffer = (char *)search_name;
    bind_query[0].buffer_length = len_search_name;
    bind_query[0].length = &len_search_name;

    if(db_stmt_set(&stmt, query, length, bind_query) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    unsigned long len_out_name = 0;
    unsigned long len_out_email = 0;

    MYSQL_BIND bind_data[3];
    memset(bind_data, 0, sizeof(bind_data));

    bind_data[0].buffer_type = MYSQL_TYPE_LONG;
    bind_data[0].buffer = &(user->id);
    bind_data[0].buffer_length = sizeof(user->id);

    bind_data[1].buffer_type = MYSQL_TYPE_STRING;
    bind_data[1].buffer = user->full_name;
    bind_data[1].buffer_length = sizeof(user->full_name);
    bind_data[1].length = &len_out_name;

    bind_data[2].buffer_type = MYSQL_TYPE_STRING;
    bind_data[2].buffer = user->email;
    bind_data[2].buffer_length = sizeof(user->email);
    bind_data[2].length = &len_out_email;

    if(mysql_stmt_bind_result(stmt, bind_data)){
        fprintf(stderr, "Failed to link results: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }

    if(mysql_stmt_store_result(stmt)){
        fprintf(stderr, "Failed to transport results: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }

    int fetch_res = mysql_stmt_fetch(stmt);

    if(fetch_res == MYSQL_NO_DATA){
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return DB_WARNING;
    }else if(fetch_res == 0 || fetch_res == MYSQL_DATA_TRUNCATED){
        user->full_name[len_out_name < sizeof(user->full_name) ? len_out_name : sizeof(user->full_name) - 1] = '\0';
        user->email[len_out_email < sizeof(user->email) ? len_out_email : sizeof(user->email) - 1] = '\0';
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return DB_SUCCESS;
    }else{
        fprintf(stderr, "Data assignment failure: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }
}

db_status_t db_edit_user(int user_id, const char *new_user_name, const char *new_user_email){

     MYSQL_STMT *stmt = NULL;

    if(db_stmt_init(&stmt) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    const char query[] = "UPDATE users SET name = ?, email = ? WHERE id = ?";
    unsigned long length = strlen(query);

    unsigned long len_name = strlen(new_user_name);
    unsigned long len_email = strlen(new_user_email);

    int local_id = user_id;

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *)new_user_name;
    bind[0].buffer_length = len_name;
    bind[0].length = &len_name;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *)new_user_email;
    bind[1].buffer_length = len_email;
    bind[1].length = &len_email;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &local_id;
    bind[2].buffer_length = sizeof(local_id);

    if(db_stmt_set(&stmt, query, length, bind) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    my_ulonglong altered_rows = mysql_stmt_affected_rows(stmt);

    if(altered_rows == 0){
        mysql_stmt_close(stmt);
        return DB_WARNING;
    }

    mysql_stmt_close(stmt);
    return DB_SUCCESS;
}

db_status_t db_delete_user(int user_id){

    MYSQL_STMT *stmt = NULL;

    if(db_stmt_init(&stmt) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    const char query[] = "DELETE FROM users WHERE id = ?";
    unsigned long length = strlen(query);
    
    int local_id = user_id;

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &local_id;
    bind[0].buffer_length = sizeof(local_id);

    if(db_stmt_set(&stmt, query, length, bind) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    my_ulonglong altered_rows = mysql_stmt_affected_rows(stmt);

    if(altered_rows == 0){
        mysql_stmt_close(stmt);
        return DB_WARNING;
    }

    mysql_stmt_close(stmt);
    return DB_SUCCESS;
}

db_status_t db_list_users(void) {
    int res_id;
    char res_name[100];
    char res_email[100];
    
    unsigned long len_out_name = 0;
    unsigned long len_out_email = 0;

    MYSQL_STMT *stmt = NULL;
    
    if (db_stmt_init(&stmt) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR;

    const char query[] = "SELECT id, name, email FROM users;";
    unsigned long length = strlen(query);

    if (db_stmt_set(&stmt, query, length, NULL) == DB_CRITICAL_ERROR) return DB_CRITICAL_ERROR; 

    MYSQL_BIND bind_data[3];
    memset(bind_data, 0, sizeof(bind_data));

    bind_data[0].buffer_type = MYSQL_TYPE_LONG;
    bind_data[0].buffer = &res_id;
    bind_data[0].buffer_length = sizeof(res_id);

    bind_data[1].buffer_type = MYSQL_TYPE_STRING;
    bind_data[1].buffer = res_name;
    bind_data[1].buffer_length = sizeof(res_name);
    bind_data[1].length = &len_out_name;

    bind_data[2].buffer_type = MYSQL_TYPE_STRING;
    bind_data[2].buffer = res_email;
    bind_data[2].buffer_length = sizeof(res_email);
    bind_data[2].length = &len_out_email;

    if (mysql_stmt_bind_result(stmt, bind_data)) {
        fprintf(stderr, "Failed to link results: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }

    if (mysql_stmt_store_result(stmt)) {
        fprintf(stderr, "Failed to transport results: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }

    int fetch_status;
    int count = 0;

    printf("\n=================== LISTAGEM DE USUÁRIOS ===================\n");
    
    while ((fetch_status = mysql_stmt_fetch(stmt)) == 0) {
        res_name[len_out_name < sizeof(res_name) ? len_out_name : sizeof(res_name) - 1] = '\0';
        res_email[len_out_email < sizeof(res_email) ? len_out_email : sizeof(res_email) - 1] = '\0';

        printf("ID: %d | Nome: %s | Email: %s\n", res_id, res_name, res_email);
        count++;
    }

    mysql_stmt_free_result(stmt);

    if (fetch_status == MYSQL_NO_DATA) {
        printf("============================================================\n");
        mysql_stmt_close(stmt);
        
        if (count == 0) {
            printf("Nenhum registro encontrado na base de dados.\n");
            printf("============================================================\n");
            return DB_WARNING;
        }
        return DB_SUCCESS;
    } else {
        fprintf(stderr, "Data assignment failure: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return DB_CRITICAL_ERROR;
    }
}