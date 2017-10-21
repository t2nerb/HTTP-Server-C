#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include<sys/types.h>
#include<errno.h>
#include<unistd.h>
#include<sys/stat.h>
#include<string.h>

#define MAX_FIELD_LEN 256
#define NTYPES 32
#define MAX_PATH_LEN 128


struct ConfigData {
    int port;
    char root_dir[MAX_FIELD_LEN];
    char default_page[MAX_FIELD_LEN];
    char extensions[NTYPES][MAX_FIELD_LEN];
    char http_enc[NTYPES][MAX_FIELD_LEN];
};

// Function declarations
void config_parse(struct ConfigData *config_data);
void remove_elt(char *og_str, const char *sub_str);
int config_socket(struct ConfigData config_data);
