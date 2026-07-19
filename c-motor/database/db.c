#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "../protocol.h"
#include "config.h"

static MYSQL *global_connection = NULL;

void db_init(const db_config_t *config){
    global_connection = mysql_init(NULL);

    char query[100] = {0};

    if(mysql_real_connect(global_connection, config->host,
                                             config->user,
                                             config->pass,
                                             NULL,
                                             config->port,
                                             NULL,
                                             0) == NULL){
                                                
        fprintf(stderr, "Failed to connect to the database: %s", mysql_error(global_connection));
        exit(1);
    }

    snprintf(query, sizeof(query), "CREATE DATABASE IF NOT EXISTS `%s`", config->db_name);

    if(mysql_query(global_connection, query)){
        fprintf(stderr, "Database creation failed: %s\n", mysql_error(global_connection));
        exit(1);
    }

    if(mysql_select_db(global_connection, config->db_name)){
        fprintf(stderr, "Failed to select the database: %s\n", mysql_error(global_connection));
        exit(1);
    }

    if(mysql_query(global_connection, "SET GLOBAL innodb_flush_log_at_trx_commit = 2;")){
        fprintf(stderr, "It was not possible to set the InnoDB flush optimization: %s\n", mysql_error(global_connection));
    }

    if(mysql_query(global_connection, "CREATE TABLE IF NOT EXISTS users (\
                                                    id INT AUTO_INCREMENT PRIMARY KEY,\
                                                    name VARCHAR(100) NOT NULL,\
                                                    email VARCHAR(100) UNIQUE NOT NULL,\
                                                    access_level TINYINT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB")){
        
        fprintf(stderr, "Table creation failed: %s\n", mysql_error(global_connection));
        exit(1);
    }

}

void db_close(void){
    mysql_close(global_connection);
}

int db_insert_user(const user_protocol_t *user){

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);

    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return -1;
    }

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

    if(mysql_stmt_prepare(stmt_user, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_bind_param(stmt_user, bind)){
        fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    mysql_stmt_close(stmt_user);

    return 0;

}

int db_find_user(user_data_t *user, const char *search_name){

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);

    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return -1;
    }

    const char query[] = "SELECT id, name, email FROM users WHERE name = ?";
    unsigned long length = strlen(query);
    unsigned long len_search_name = strlen(search_name);

    MYSQL_BIND bind_query[1];
    memset(bind_query, 0, sizeof(bind_query));

    bind_query[0].buffer_type = MYSQL_TYPE_STRING;
    bind_query[0].buffer_length = len_search_name;
    bind_query[0].length = &len_search_name;
    bind_query[0].buffer = (char *)search_name;

    if(mysql_stmt_prepare(stmt_user, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_bind_param(stmt_user, bind_query)){
        fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    MYSQL_BIND bind_data[3];
    memset(bind_data, 0, sizeof(bind_data));

    bind_data[0].buffer_type = MYSQL_TYPE_LONG;
    bind_data[0].buffer_length = sizeof(user->id);
    bind_data[0].buffer = &(user->id);

    bind_data[1].buffer_type = MYSQL_TYPE_STRING;
    bind_data[1].buffer_length = sizeof(user->full_name);
    bind_data[1].buffer = user->full_name;

    bind_data[2].buffer_type = MYSQL_TYPE_STRING;
    bind_data[2].buffer_length = sizeof(user->email);
    bind_data[2].buffer = user->email;

    if(mysql_stmt_bind_result(stmt_user, bind_data)){
        fprintf(stderr, "Failed to link results: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_store_result(stmt_user)){
        fprintf(stderr, "Failed to transport results: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    int fetch_res = mysql_stmt_fetch(stmt_user);

    if(fetch_res == MYSQL_NO_DATA){
        mysql_stmt_close(stmt_user);
        return 1;
    }else if(fetch_res == 0){
        mysql_stmt_close(stmt_user);
        return 0;
    }else{
        fprintf(stderr, "Data assignment failure: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

}

int db_edit_user(int user_id, const char *new_user_name, const char *new_user_email){

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);

    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return -1;
    }

    const char query[] = "UPDATE users SET name = ?, email = ? WHERE id = ?";
    unsigned long length = strlen(query);

    unsigned long len_name = strlen(new_user_name);
    unsigned long len_email = strlen(new_user_email);

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer_length = len_name;
    bind[0].length = &len_name;
    bind[0].buffer = (char *)new_user_name;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer_length = len_email;
    bind[1].length = &len_email;
    bind[1].buffer = (char *)new_user_email;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &user_id;

    if(mysql_stmt_prepare(stmt_user, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_bind_param(stmt_user, bind)){
        fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    my_ulonglong altered_rows = mysql_stmt_affected_rows(stmt_user);

    if(altered_rows == 0){
        mysql_stmt_close(stmt_user);
        return 1;
    }

    mysql_stmt_close(stmt_user);

    return 0;

}

int db_delete_user(int user_id){

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);

    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return -1;
    }

    const char query[] = "DELETE FROM users WHERE id = ?";
    unsigned long length = strlen(query);

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &user_id;

    if(mysql_stmt_prepare(stmt_user, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_bind_param(stmt_user, bind)){
        fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    my_ulonglong altered_rows = mysql_stmt_affected_rows(stmt_user);

    if(altered_rows == 0){
        mysql_stmt_close(stmt_user);
        return 1;
    }

    mysql_stmt_close(stmt_user);

    return 0;

}

int db_list_users(void){
    int res_id;
    char res_name[100];
    char res_email[100];

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);
    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return -1;
    }

    const char query[] = "SELECT id, name, email FROM users;";
    unsigned long length = strlen(query);

    if(mysql_stmt_prepare(stmt_user, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    MYSQL_BIND bind_data[3];
    memset(bind_data, 0, sizeof(bind_data));

    bind_data[0].buffer_type = MYSQL_TYPE_LONG;
    bind_data[0].buffer = &res_id;

    bind_data[1].buffer_type = MYSQL_TYPE_STRING;
    bind_data[1].buffer = res_name;
    bind_data[1].buffer_length = sizeof(res_name);

    bind_data[2].buffer_type = MYSQL_TYPE_STRING;
    bind_data[2].buffer = res_email;
    bind_data[2].buffer_length = sizeof(res_email);

    if(mysql_stmt_bind_result(stmt_user, bind_data)){
        fprintf(stderr, "Failed to link results: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_store_result(stmt_user)){
        fprintf(stderr, "Failed to transport results: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    int fetch_status;
    int count = 0;

    printf("\n=================== LISTAGEM DE USUÁRIOS ===================\n");
    
    while ((fetch_status = mysql_stmt_fetch(stmt_user)) == 0) {
        printf("ID: %d | Nome: %s | Email: %s\n", res_id, res_name, res_email);
        count++;
    }

    if (fetch_status == MYSQL_NO_DATA) {
        if (count == 0) {
            printf("Nenhum registro encontrado na base de dados.\n");
            printf("============================================================\n");
            mysql_stmt_close(stmt_user);
            return 1;
        }
        printf("============================================================\n");
        mysql_stmt_close(stmt_user);
        return 0;
    } else {
        fprintf(stderr, "Data assignment failure: %s\n", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }
}