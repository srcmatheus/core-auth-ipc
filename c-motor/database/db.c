#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

static MYSQL * global_connection = NULL;

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