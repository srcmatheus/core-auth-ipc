#include <stdio.h>
#include <stdlib.h>
#include "config.h"

void config_init(db_config_t * config){

    config->host = getenv("DB_HOST");
    config->port = getenv("DB_PORT");
    config->user = getenv("DB_USER");
    config->db_name = getenv("DB_NAME");
    config->pass = getenv("DB_PASS");

    if(config->host == NULL || config->host[0] == '\0') config->host = "localhost";
    if(config->port == NULL || config->port[0] == '\0') config->host = "3306";
    if(config->user == NULL || config->user[0] == '\0') config->user = "root";

    if(config->db_name == NULL || config->db_name[0] == '\0') config->db_name = "core_auth_database";

    if(config->pass == NULL || config->pass[0] == '\0'){
        fprintf(stderr, "Fatal error: The DB_PASS variable was not defined in the environment.\n");
        exit(1);
    }

}