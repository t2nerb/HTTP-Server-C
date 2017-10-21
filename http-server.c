#include "http-server.h"


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

    // Loop and wait for a new connection
    // When there's a new connection create a new child process
    //      Close listening socket in child process address space to return control to parent process
    //      Trigger child_handler to parse URI and send corresponding response back to client
    for(;;)
    {
        // Local vars
        int clientfd;
        int pid;

        // Accept new connections and store new file descriptoer into clientfd
        clientfd = accept(sockfd, (struct sockaddr *) &client, &client_len);
        if (clientfd < 0) {
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
            printf("PID[%d] ", getpid());
            child_handler(clientfd, &config_data);
            exit(0);
        }

        // I am parent
        if (pid > 0) {
            close(clientfd);

            // Wait for change in process state and wait
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
    while (fgets(conf_line, MAX_FIELD_LEN, config_file))
    {
        // Local vars
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
    printf("DEFAULT PAGES: %s\n\n", config_data->default_page);

}


// Remove substring from a char array
void remove_elt(char *og_str, const char *sub_str)
{
    while((og_str=strstr(og_str,sub_str)))
    {
        memmove(og_str,og_str+strlen(sub_str),1+strlen(og_str+strlen(sub_str)));
    }
}


// Bind to socket and listen on port
int config_socket(struct ConfigData config_data)
{
    // Local Vars
    int sockfd;
    int enable = 1;
    struct sockaddr_in remote;

    // Populate sockaddr_in struct with configuration for socket
    bzero(&remote, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(config_data.port);
    remote.sin_addr.s_addr = INADDR_ANY;

    // Create a socket of type TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not create socket: ");
        exit(-1);
    }

    // Set socket option for fast-rebind socket
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        perror("Unable to set sock option: ");

    if ((bind(sockfd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in))) < 0) {
        perror("Could not bind to socket: ");
        exit(-1);
    }

    // Allow up to 4 simultaneous connections on the socket
    if ((listen(sockfd, 4)) < 0) {
        perror("Could not listen: ");
        exit(-1);
    }

    return sockfd;
}


// Send appropriate message to client based on req_code
void send_header(int clientfd, int req_code, struct ReqParams *req_params)
{
    char response[MAX_BUF_SIZE];
    char not_implemented[] = "HTTP/1.1 501 Not Implemented \r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html><html><head><title>501 Not Implemented</title>"
    "<body>501 Implementation missing </body></html>\r\n";



    snprintf(response, sizeof(response), not_implemented, req_params->uri, req_params->uri);
    send(clientfd, response, sizeof(response), 0);
}


// Primary procedure to handle all HTTP requests
void child_handler(int clientfd, struct ConfigData *config_data)
{
    // Local Vars
    char recv_buff[MAX_BUF_SIZE];
    int recv_len;
    int req_code;
    struct ReqParams req_params;

    // Receive the data sent by client
    recv_len = recv(clientfd, recv_buff, MAX_BUF_SIZE, 0);

    // Parse the method, URI, version from request and print the fields
    parse_request(recv_buff, &req_params);

    // Error checking for URI, version and method
    //      (-1 = methodnotsupported, -2 = versionnotsupported, -3 = file_no_exist)
    req_code = check_request(&req_params, config_data);
    if (req_code > 200) {
        send_header(clientfd, req_code, &req_params);
        // send error content here
    } else {
        send_header(clientfd, req_code, &req_params);
        // Send file content here
    }

    printf("%s %s %s\n", req_params.method, req_params.uri, req_params.version);
    printf("  %d\n", req_code);

}


// Parse the request header contained in recv_buff and store into req_params
// *** Note: Only need to parse first line containing method, URI, version
void parse_request(char *recv_buff, struct ReqParams *req_params)
{
    // Local Vars
    char *token;
    char *line;
    char *field;

    // Split lines
    token = strtok(recv_buff,"\n");

    // Only relevant information is in the first line
    line = strdup(token);

    // Parse the three fields delimited by space in the line
    field = strtok(line, " ");
    req_params->method = strdup(field);
    field = strtok(NULL, " ");
    req_params->uri = strdup(field);
    field = strtok(NULL, " ");
    req_params->version = strdup(field);
    free(line);
}


// Check request parameters for erorrs
//      Return 4001 if method is not supported
//      Return 4002 if version is not supported
//      Return 404 if file is not found
int check_request(struct ReqParams *req_params, struct ConfigData *config_data)
{
    // Local Vars
    char fpath[MAX_BUF_SIZE];
    char *extension;
    int supported = 0;

    // Check for method, version errors
    if(strncmp(req_params->method, "GET", 3) != 0) {
        return 4001;
    }
    if (strncmp(req_params->version, "HTTP/1", 6) != 0) {
        return 4002;
    }

    // Create path with respect to root directory in config
    strcpy(fpath, config_data->root_dir);
    strcat(fpath, req_params->uri);

    // If no filename is included, then default to index.html
    if (strcmp(fpath, "./www/") == 0) {
        strcat(fpath, "index.html");
        if (access(fpath, F_OK) != -1 ) {
            return 200;
        } else {
            return 404;
        }
    }

    // If there is content in the uri, check if file in uri exists
    else {
        if (access(fpath, F_OK) != -1) {
            extension = strrchr(fpath, '.');

            // Check if extension is in array of supported extensions
            for (int i = 0; i < NTYPES-1; i++) {
                if (strcmp(extension, config_data->extensions[i]) == 0)
                    supported = 1;
            }

            if (supported)
                return 200;
            else
                return 501;

        } else {
            return 404;
        }
    }

    return 0;
}
