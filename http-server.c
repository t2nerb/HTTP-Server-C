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

    printf("\n******************** HTTP SERVER ***********************\n");
    printf("* LISTENING ON PORT: %d\n", config_data.port);
    printf("* SERVER DIRECTORY:  %s\n", config_data.root_dir);
    printf("* DEFAULT PAGE:      %s\n", config_data.default_page);
    printf("********************************************************\n\n");

    // Loop and wait for a new connection
    // When there's a new connection create a new child process
    //      Close listening socket in child process address space to return control to parent process
    //      Trigger child_handler to parse URI and send corresponding response back to client
    while(1)
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
        // Local Vars
        char *saveptr;
        char *firstptr;

        // Don't parse comments
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


// Primary procedure to handle all HTTP requests
void child_handler(int clientfd, struct ConfigData *config_data)
{
    // Local Vars
    char recv_buff[MAX_BUF_SIZE];
    int req_code;
    struct ReqParams req_params;

    // Receive the data sent by client
    if (recv(clientfd, recv_buff, MAX_BUF_SIZE, 0) > 0) {

        // Parse the method, URI, version from client request and store into struct
        parse_request(recv_buff, &req_params);

        // Produce HTTP code for the request (i.e 200, 400, 404, 501)
        req_code = check_request(&req_params, config_data);

        // Serialize and send response header
        send_header(clientfd, req_code, &req_params, config_data);

        // Send the relevant data to client
        send_contents(clientfd, req_code, &req_params, config_data);

        // Print all the good stuff
        printf("%d %s %s %s\n", req_code, req_params.method, req_params.uri, req_params.version);
    }
    else {
        printf("No data was received!\n");
    }

}


// Remove char from a char array
void remove_elt(char *og_str, const char *sub_str)
{
    while((og_str=strstr(og_str,sub_str)))
    {
        memmove(og_str,og_str+strlen(sub_str),1+strlen(og_str+strlen(sub_str)));
    }
}


// Send message to client and serialize header according to req_code
void send_header(int clientfd, int req_code, struct ReqParams *req_params, struct ConfigData *config_data)
{
    // Local Vars
    char response[MAX_BUF_SIZE];
    char *extension;
    int endex;

    // General format for each HTTP code
    char code_200[] = "%s 200 OK\r\nContent-Type: %s; charset=UTF-8\r\n\r\n";
    char code_400[] = "%s 400 Bad Request \r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
    char code_404[] = "%s 404 Not Found \r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";
    char code_501[] = "%s 501 Not Implemented \r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";

    // Serialize header based on req_code
    if (req_code == 200) {
        extension = strrchr(req_params->uri, '.');
        for (int i = 0; i < NTYPES-1; i++) {
            if (strcmp(extension, config_data->extensions[i]) == 0) endex = i;
        }
        // printf("config_data: %s\n", config_data->http_enc[endex]);
        // printf("req_param:   %s\n", req_params->ctype);
        snprintf(response, sizeof(response), code_200, req_params->version, config_data->http_enc[endex]);
    }

    else if (req_code == 4001 || req_code == 4002) {
        snprintf(response, sizeof(response), code_400, req_params->version, req_params->uri);
    }

    else if (req_code == 404) {
        snprintf(response, sizeof(response), code_404, req_params->version, req_params->uri);
    }

    else if (req_code == 501) {
        snprintf(response, sizeof(response), code_501, req_params->version, req_params->uri);
    }

    // Send response to client
    send(clientfd, response, strlen(response), 0);
}



// Send relevant data to
void send_contents(int clientfd, int req_code, struct ReqParams *req_params, struct ConfigData *config_data)
{
    // Local Vars
    unsigned int rbytes = 0;
    char filebuf[MAX_BUF_SIZE];
    char fpath[MAX_BUF_SIZE];
    FILE *inputfp;

    // Default messages for codes (400, 404, 500, 501)
    char code_4001[] = "<html><body> Bad Request: Unsupported protocol: %s </body></html>\r\n";
    char code_4002[] = "<html><body> Bad Request: Bad URI: %s </body></html>\r\n";
    char code_404[] = "<html><body> Not Found; URL does not exist: %s </body></html>\r\n";
    char code_501[] = "<html><body>501 Not Supported: %s </body></html>\r\n";

    // Create filepath with respect to root directory in config
    strcpy(fpath, config_data->root_dir);
    strcat(fpath, req_params->uri);

    if (req_code == 200) {
        inputfp = fopen(fpath, "rb");
        while(!feof(inputfp)) {
            rbytes = fread(filebuf, 1, MAX_BUF_SIZE, inputfp);
            send(clientfd, filebuf, rbytes, 0);
        }
        fclose(inputfp);
    }

    else if (req_code == 4001) {
        snprintf(filebuf, sizeof(filebuf), code_4001, req_params->version);
        send(clientfd, filebuf, strlen(filebuf), 0);
    }
    else if (req_code == 4002) {
        snprintf(filebuf, sizeof(filebuf), code_4002, req_params->uri);
        send(clientfd, filebuf, strlen(filebuf), 0);
    }
    else if (req_code == 404) {
        snprintf(filebuf, sizeof(filebuf), code_404, fpath);
        send(clientfd, filebuf, strlen(filebuf), 0);
    }
    else if(req_code == 501) {
        snprintf(filebuf, sizeof(filebuf), code_501, fpath);
        send(clientfd, filebuf, strlen(filebuf), 0);
    }

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

    // Remove first char from uri
    memmove (req_params->uri, req_params->uri+1, strlen (req_params->uri+1) + 1);

    // Ugh stupid trailing random bytes
    req_params->version[8] = '\0';

    free(line);
}


// Check request parameters for erorrs
//      Return 200 if file found and supported
//      Return 4001 if method is not supported
//      Return 4002 if version is not supported
//      Return 404 if file is not found
//      Return 501 if file found but not supported
int check_request(struct ReqParams *req_params, struct ConfigData *config_data)
{
    // Local Vars
    char fpath[MAX_BUF_SIZE];
    char *extension;
    int supported = 0;

    // Only account for GET method
    if(strncmp(req_params->method, "GET", 3) != 0) {
        return 4001;
    }

    // Only support HTTP/1.x
    if (strncmp(req_params->version, "HTTP/1", 6) != 0) {
        return 4002;
    }

    // Create path with respect to root directory in config
    strcpy(fpath, config_data->root_dir);
    strcat(fpath, req_params->uri);

    // If no filename is included, then set to default in ws.conf
    if (strcmp(fpath, config_data->root_dir) == 0) {
        strcat(fpath, config_data->default_page);
        req_params->uri = config_data->default_page;

        return (access(fpath, F_OK) != -1) ? 200 : 404;
    }
    // If there is content in the uri, check if file in uri exists
    else {
        if (access(fpath, F_OK) != -1) {
            extension = strrchr(fpath, '.');

            // Check if extension is in the list of supported file types
            for (int i = 0; i < NTYPES-1; i++) {
                if (strcmp(extension, config_data->extensions[i]) == 0) {
                    supported = 1;
                    // req_params->ctype = malloc(sizeof(config_data->http_enc[i])+1);
                    // req_params->ctype = config_data->http_enc[i];
                    // printf("ctype: %s\n", req_params->ctype);
                }
            }

            return (supported) ? 200 : 501;

        } else {
            return 404;
        }
    }

    return 0;
}
