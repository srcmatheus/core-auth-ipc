#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

#include "config.h"
#include "db_utils.h"

static MYSQL *global_connection = NULL;

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
                                             0) == NULL){
                                                
        fprintf(stderr, "Failed to connect to the database: %s", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
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

    if(mysql_query(global_connection, "SET GLOBAL innodb_flush_log_at_trx_commit = 2;")){
        fprintf(stderr, "It was not possible to set the InnoDB flush optimization: %s\n", mysql_error(global_connection));
    }

    if(mysql_query(global_connection, "CREATE TABLE IF NOT EXISTS users (\
                                                    id INT AUTO_INCREMENT PRIMARY KEY,\
                                                    name VARCHAR(100) NOT NULL,\
                                                    email VARCHAR(100) UNIQUE NOT NULL,\
                                                    access_level TINYINT UNSIGNED NOT NULL DEFAULT 0) ENGINE=InnoDB")){
        
        fprintf(stderr, "Table creation failed: %s\n", mysql_error(global_connection));
        mysql_close(global_connection);
        exit(1);
    }

}
void db_close(void){
    if(global_connection != NULL) {
        mysql_close(global_connection);
        global_connection = NULL;
    }
}

db_status_t db_stmt_init(MYSQL_STMT **stmt){

    *stmt = mysql_stmt_init(global_connection);

    if(*stmt == NULL){
        fprintf(stderr, "Error connecting to the database: %s\n", mysql_error(global_connection));
        return DB_CRITICAL_ERROR;
    }

    return DB_SUCCESS;
}
db_status_t db_stmt_set(MYSQL_STMT **stmt, const char *query, unsigned long length, MYSQL_BIND *bind){

    if(mysql_stmt_prepare(*stmt, query, length)){
        fprintf(stderr, "Error during preparation: %s\n", mysql_stmt_error(*stmt));
        mysql_stmt_close(*stmt);
        *stmt = NULL;
        return DB_CRITICAL_ERROR;
    }

    if(bind != NULL) {
        if (mysql_stmt_bind_param(*stmt, bind)) {
            fprintf(stderr, "Bind error: %s\n", mysql_stmt_error(*stmt));
            mysql_stmt_close(*stmt);
            *stmt = NULL;
            return DB_CRITICAL_ERROR;
        }
    }

    if(mysql_stmt_execute(*stmt)){
        fprintf(stderr, "Error executing query: %s\n", mysql_stmt_error(*stmt));
        mysql_stmt_close(*stmt);
        *stmt = NULL;
        return DB_CRITICAL_ERROR;
    }

    return DB_SUCCESS;
}

//Test ===============================================================================================

void autocommit(int mode){
    mysql_autocommit(global_connection, mode);
}

void commit(void){
    mysql_commit(global_connection);
}