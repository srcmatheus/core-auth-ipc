#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mysql/mysql.h>

#include "config.h"
#include "db_utils.h"

static MYSQL *global_connection = NULL;

db_prepared_stmts_t db_stmts = {0};

static MYSQL_STMT* prepare_query(const char* query){

    MYSQL_STMT *stmt = mysql_stmt_init(global_connection);

    if(!stmt){
        fprintf(stderr, "Error initializing statement: %s\n", mysql_error(global_connection));
        exit(1);
    }

    if(mysql_stmt_prepare(stmt, query, strlen(query))){
        fprintf(stderr, "Error preparing the query '%s': %s\n", query, mysql_stmt_error(stmt));
        exit(1);
    }

    return stmt;
}

void db_init(const db_config_t *config){

    global_connection = mysql_init(NULL);
    
    if(global_connection == NULL) {
        fprintf(stderr, "Fatal: mysql_init failed (Out of memory)\n");
        exit(1);
    }

    if(mysql_real_connect(global_connection, config->host,
                                             config->user,
                                             config->pass,
                                             NULL,
                                             config->port,
                                             NULL,
                                             CLIENT_FOUND_ROWS) == NULL){
                                                
        fprintf(stderr, "Failed to connect to the database: %s", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
    }

    if (mysql_set_character_set(global_connection, "utf8mb4")) {
        fprintf(stderr, "Warning: Could not set the utf8mb4 charset: %s\n", mysql_error(global_connection));
    }

    char query[128] = {0};

    snprintf(query, sizeof(query), "CREATE DATABASE IF NOT EXISTS `%s`", config->db_name);

    if(mysql_query(global_connection, query)){
        fprintf(stderr, "Database creation failed: %s\n", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
    }

    if(mysql_select_db(global_connection, config->db_name)){
        fprintf(stderr, "Failed to select the database: %s\n", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
    }

    if(mysql_query(global_connection, "CREATE TABLE IF NOT EXISTS users (\
                                            id INT AUTO_INCREMENT PRIMARY KEY,\
                                            name VARCHAR(100) NOT NULL,\
                                            email VARCHAR(100) UNIQUE NOT NULL,\
                                            access_level TINYINT UNSIGNED NOT NULL DEFAULT 0,\
                                            INDEX idx_name_id (name, id)\
                                            ) ENGINE=InnoDB")){
        
        fprintf(stderr, "Table creation failed: %s\n", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
    }

    db_stmts.insert_user = prepare_query("INSERT INTO users(name, email) VALUES(?, ?)");
    db_stmts.find_user = prepare_query("SELECT id, name, email, access_level FROM users "
                                         "WHERE name LIKE ? AND (name > ? OR (name = ? AND id > ?)) "
                                         "ORDER BY name ASC, id ASC LIMIT 50");
    db_stmts.edit_user = prepare_query("UPDATE users SET name = ?, email = ?, access_level = ? WHERE id = ?");
    db_stmts.delete_user = prepare_query("DELETE FROM users WHERE id = ?");

}
void db_close(void){

    if(db_stmts.insert_user) mysql_stmt_close(db_stmts.insert_user);
    if(db_stmts.find_user) mysql_stmt_close(db_stmts.find_user);
    if(db_stmts.edit_user) mysql_stmt_close(db_stmts.edit_user);
    if(db_stmts.delete_user) mysql_stmt_close(db_stmts.delete_user);

    if(global_connection != NULL) {
        mysql_close(global_connection);
        global_connection = NULL;
    }
}

db_status_t db_execute_stmt(MYSQL_STMT *stmt, MYSQL_BIND *bind) {

    if(bind != NULL) {
        if (mysql_stmt_bind_param(stmt, bind)) {
            fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(stmt));
            return DB_CRITICAL_ERROR;
        }
    }

    if(mysql_stmt_execute(stmt)) {
        fprintf(stderr, "Execution error: %s\n", mysql_stmt_error(stmt));
        return DB_CRITICAL_ERROR;
    }

    return DB_SUCCESS;
}

db_status_t db_ping(const db_config_t *config){

    if(mysql_ping(global_connection) == 0){
        return DB_SUCCESS;
    }

    db_close();

    db_init(config);

    return DB_SUCCESS;

}