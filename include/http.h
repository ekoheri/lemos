#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char *method;
    char *path;
    char *protocol;
    char *http_version;
    char *host;
    int port;
    char *request_time;
    char *encrypted;
    char *form_data;
    int content_length;
} RequestHeader;

typedef struct {
    char *http_version;     // Untuk menyimpan HTTP version
    char *status_code;      // Untuk menyimpan status code
    char *content_type;     // Untuk menyimpan Content-Type
    unsigned long content_length; //  Untuk menyimpan Content-Length
    char *connection;       // Untuk menyimpan Connection
    char *cache_control;    // Untuk menyimpan Cache-Control
    char *encrypted;        // Untuk menyimpan Encrypted
    char *response_time;    // Untuk menyimpan Response-Time
} ResponseHeaders;

#define BUFFER_SIZE 1024

//void handle_url(char *url, char *hostname, char *path, int *port, char *protocol);
char *handle_response(char *url, char *form_data);

#endif
