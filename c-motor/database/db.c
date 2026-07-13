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
                                             NULL, 0, NULL, 0) == NULL){
                                                
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

    if(mysql_query(global_connection, "CREATE TABLE IF NOT EXISTS users (\
                                                    id INT AUTO_INCREMENT PRIMARY KEY,\
                                                    name VARCHAR(100) NOT NULL,\
                                                    email VARCHAR(100) UNIQUE NOT NULL,\
                                                    access_level TINYINT UNSIGNED NOT NULL DEFAULT 0)")){
        
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