#include "http-server.h"

// Function declarations
void config_parse(struct ConfigData *config_data);
void remove_elt(char *og_str, const char *sub_str);
int config_socket(struct ConfigData config_data);


int main(int argc, char** argv)
{
    // Local vars
    int sockfd;
    unsigned int client_len;
    struct sockaddr_in client;
    struct ConfigData config_data;
    client_len = sizeof(struct sockaddr_in);

    // Read in configuration file to struct
    config_parse(&config_data);

    // Bind the socket and listen on specified port
    sockfd = config_socket(config_data);

    while(1)
    {
        // Local vars
        int nsfd, pid;

        // Accept new connections and store new file descriptoer into nsfd
        if ((nsfd = accept(sockfd, (struct sockaddr *) &client, &client_len)) < 0) {
            perror("Could not accept: ");
            exit(-1);
        }

        pid = fork();
        if (pid < 0) {
            perror("Could not fork: ");
            exit(-1);
        }

        // I am the child
        if (pid == 0) {
            close(sockfd);
            printf("Hi something happened!\n");
            exit(0);
        }

        if (pid > 0) {
            close(nsfd);
            waitpid(0, NULL, WNOHANG);
        }
    }

    return 0;
}


// Parse the ws.conf file and store the data into a struct
void config_parse(struct ConfigData *config_data)
{
    // Local vars
    char conf_line[MAX_FIELD_LEN];
    FILE *config_file;

    config_file = fopen("./ws.conf", "r");
    if (!config_file) {
        perror("Could not find ./ws.conf");
        exit(-1);
    }

    // Loop through file and store relevant information into struct
    int counter = 0, fcounter = 0;
    while (fgets (conf_line, MAX_FIELD_LEN, config_file))
    {
        char *saveptr;
        char *firstptr;

        // Ignore comments in configuration file
        if (conf_line[0] != '#') {
            if (counter == 0) {
                strtok_r(conf_line, " ", &saveptr);
                config_data->port = atoi(saveptr);
            }
            else if (counter == 1) {
                strtok_r(conf_line, " ", &saveptr);
                remove_elt(saveptr, "\"");
                remove_elt(saveptr, "\n");
                strcpy(config_data->root_dir, saveptr);
            }
            else if (counter == 2) {
                strtok_r(conf_line, " ", &saveptr);
                remove_elt(saveptr, "\n");
                strcpy(config_data->default_page, saveptr);
            }
            else {
                firstptr = strtok_r(conf_line, " ", &saveptr);
                strcpy(config_data->extensions[fcounter], firstptr);
                remove_elt(saveptr, "\n");
                strcpy(config_data->http_enc[fcounter], saveptr);
                fcounter++;
            }
            counter++;
        }

    }
    fclose(config_file);

    printf("PORT: %d\n", config_data->port);
    printf("DIRECTORY: %s\n", config_data->root_dir);
    printf("DEFAULT PAGES: %s\n", config_data->default_page);

}


// Remove a substring from a char array
void remove_elt(char *og_str, const char *sub_str)
{
    while((og_str=strstr(og_str,sub_str)))
        memmove(og_str,og_str+strlen(sub_str),1+strlen(og_str+strlen(sub_str)));
}


// Bind to socket and listen on port
int config_socket(struct ConfigData config_data)
{
    int sockfd;
    struct sockaddr_in remote;

    bzero(&remote, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(config_data.port);
    remote.sin_addr.s_addr = INADDR_ANY;
    // memset(remote.sin_zero, '\0', sizeof(remote.sin_zero));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not allocate socket: ");
        exit(-1);
    }

    if ((bind(sockfd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in))) < 0) {
        perror("Could not bind to socket: ");
        exit(-1);
    }

    if ((listen(sockfd, 4)) < 0) {
        perror("Could not listen: ");
        exit(-1);
    }

    return sockfd;
}
