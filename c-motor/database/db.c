#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

#include "protocol.h"

static MYSQL *global_connection = NULL;

void db_init(void){
    global_connection = mysql_init(NULL);

    if(mysql_real_connect(global_connection, "localhost", "usuario", "senha", "nome_do_banco_de_dados", 0, NULL, 0) == NULL){
        fprintf(stderr, "Failed to connect to the database: %s ", mysql_error(global_connection));
        exit(1);
    }
}

void db_close(void){
    mysql_close(global_connection);
}

int db_insert_user(const user_protocol_t *user){

    MYSQL_STMT *stmt_user = mysql_stmt_init(global_connection);

    if(stmt_user == NULL){
        fprintf(stderr, "Error connecting to the database: %s ", mysql_error(global_connection));
        return -1;
    }

    const char query[] = "INSERT INTO users(nome, email) VALUES(?, ?)";
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
        fprintf(stderr, "Error during preparation: %s ", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_bind_param(stmt_user, bind)){
        fprintf(stderr, "Bind error: %s ", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    if(mysql_stmt_execute(stmt_user)){
        fprintf(stderr, "Error executing query: %s ", mysql_stmt_error(stmt_user));
        mysql_stmt_close(stmt_user);
        return -1;
    }

    mysql_stmt_close(stmt_user);

    return 0;

}