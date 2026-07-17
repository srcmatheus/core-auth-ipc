#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    const char *host;
    unsigned int port;
    const char *user;
    const char *pass;
    const char *db_name;
} db_config_t;

void config_init(db_config_t *config);

#endif