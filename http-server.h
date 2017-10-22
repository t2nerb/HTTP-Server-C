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
#define MAX_BUF_SIZE 512

struct ConfigData {
    int port;
    char root_dir[MAX_FIELD_LEN];
    char default_page[MAX_FIELD_LEN];
    char extensions[NTYPES][MAX_FIELD_LEN];
    char http_enc[NTYPES][MAX_FIELD_LEN];
};

struct ReqParams {
    char *uri;
    char *version;
    char *method;
};

int config_socket(struct ConfigData config_data);
int check_request(struct ReqParams *req_params, struct ConfigData *config_data);
void config_parse(struct ConfigData *config_data);
void remove_elt(char *og_str, const char *sub_str);
void child_handler(int clientfd, struct ConfigData *config_data);
void parse_request(char *recv_buff, struct ReqParams *req_params);
void send_header(int clientfd, int req_code, struct ReqParams *req_params, struct ConfigData *config_data);
