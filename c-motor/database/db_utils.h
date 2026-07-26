#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "../protocol.h"
#include "config.h"

typedef enum {
    DB_CRITICAL_ERROR = -1,
    DB_SUCCESS = 0,
    DB_WARNING = 1
} db_status_t;

typedef struct {
    MYSQL_STMT *insert_user;
    MYSQL_STMT *find_user;
    MYSQL_STMT *edit_user;
    MYSQL_STMT *delete_user;
} db_prepared_stmts_t;

extern db_prepared_stmts_t db_stmts;

void db_init(const db_config_t *config);
void db_close(void);

db_status_t db_execute_stmt(MYSQL_STMT *stmt, MYSQL_BIND *bind);

#endif