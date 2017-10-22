HTTP Server in C
================
By: Brent Dagdagan 


About
----------------

Web server utilizing HTTP and TCP protocols. The code works by creating a socket that binds to the port specified in the configuration file, and listens for incoming client connections. When a connection is established (Client sends an HTTP request), a child-process is created to handle the request. 

The server supports GET requests and HTTP status codes: 200, 400, 404, 501 

Setup
----------------

A makefile is provided to compile the code. Execute all the following commands in the project's root directory: 

To compile: 

```bash
$ make
```

To run the server: 

```bash
$ ./http-server 
```

To cleanup: 

```bash
$ make clean
```

Configuration
----------------

By default, the application is set to serve files in `./www/`.
All configurable parameters are in `./ws.conf`.

From here you can edit configurations for:  

* Root web directory 
* Change listening port 
* Add new filetypes
