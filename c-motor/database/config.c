#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"

void config_init(db_config_t * config){

    char *port_env = getenv("DB_PORT");

    config->host = getenv("DB_HOST");
    config->user = getenv("DB_USER");
    config->db_name = getenv("DB_NAME");
    config->pass = getenv("DB_PASS");

    if(config->host == NULL || config->host[0] == '\0') config->host = "127.0.0.1";
    
    if (port_env == NULL || port_env[0] == '\0') {
        config->port = 3306;
    } else {
        char *endptr;
        errno = 0;
        long port = strtol(port_env, &endptr, 10);

        if (errno != 0 || *endptr != '\0' || port < 1 || port > 65535) {
            fprintf(stderr, "Warning: Invalid DB_PORT ('%s'). Using the default port 3306.\n", port_env);
            config->port = 3306;
        } else {
            config->port = (unsigned int)port;
        }
    }
    
    if(config->user == NULL || config->user[0] == '\0') config->user = "root";

    if(config->db_name == NULL || config->db_name[0] == '\0') config->db_name = "core_auth_database";

    if(config->pass == NULL || config->pass[0] == '\0'){
        fprintf(stderr, "Fatal error: The DB_PASS variable was not defined in the environment.\n");
        exit(1);
    }

}