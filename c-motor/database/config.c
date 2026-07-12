#include <stdio.h>
#include <stdlib.h>
#include "config.h"

void config_init(db_config_t * config){

    config->host = getenv("DB_HOST");
    config->user = getenv("DB_USER");
    config->db_name = getenv("DB_NAME");
    config->pass = getenv("DB_PASS");

    if(config->host == NULL) config->host = "localhost";
    if(config->user == NULL) config->user = "root";

    if(config->db_name == NULL){
        fprintf(stderr, "Fatal error: The DB_PASS variable was not defined in the environment.\n");
        exit(1);
    }

    if(config->pass == NULL){
        fprintf(stderr, "Fatal error: The DB_PASS variable was not defined in the environment.\n");
        exit(1);
    }

}