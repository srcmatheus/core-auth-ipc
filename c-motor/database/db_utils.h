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

void db_init(const db_config_t *config);
void db_close(void);

db_status_t db_stmt_init(MYSQL_STMT **stmt);
db_status_t db_stmt_set(MYSQL_STMT **stmt, const char *query, unsigned long length, MYSQL_BIND *bind);

void autocommit(int mode);
void commit(void);

#endif