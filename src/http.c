#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

#include "../include/http.h"
#include "../include/config.h"
#include "../include/sha256.h"
#include "../include/chacha20.h"
#include "../include/pasaran.h"

extern Config config;

RequestHeader handle_url(char *url) {
    RequestHeader request;
    request.protocol = (char *)malloc(10 * sizeof(char));
    request.host = (char *)malloc(256 * sizeof(char)); // Menyediakan cukup ruang untuk hostname
    request.path = (char *)malloc(256 * sizeof(char)); // Menyediakan cukup ruang untuk path
    request.http_version = (char *)malloc(10 * sizeof(char)); // Untuk menyimpan HTTP/1.1 atau versi lainnya
    request.request_time = (char *)malloc(128 * sizeof(char));
    request.encrypted = (char *)malloc(5 * sizeof(char));
    request.content_length = 0;

    // Default values
    strcpy(request.protocol, "http");
    strcpy(request.path, "/");
    strcpy(request.http_version, "HTTP/1.1");

    char *http_prefix = "http://";
    char *https_prefix = "https://";

    // Tentukan protocol dan port
    if (strncmp(url, http_prefix, strlen(http_prefix)) == 0) {
        strcpy(request.protocol, "HTTP");
        url += strlen(http_prefix);
        request.port = 80;
    } else if (strncmp(url, https_prefix, strlen(https_prefix)) == 0) {
        strcpy(request.protocol, "HTTPS");
        url += strlen(https_prefix);
        request.port = 443;
    }

    char *colon_pos = strchr(url, ':');
    char *slash_pos = strchr(url, '/');
    
    if (colon_pos != NULL && (slash_pos == NULL || colon_pos < slash_pos)) {
        // Mengambil hostname dan port
        strncpy(request.host, url, colon_pos - url);
        request.host[colon_pos - url] = '\0';
        request.port = atoi(colon_pos + 1);
        if (slash_pos != NULL) {
            strcpy(request.path, slash_pos);
        }
    } else if (slash_pos != NULL) {
        // Mengambil hostname dan path
        strncpy(request.host, url, slash_pos - url);
        request.host[slash_pos - url] = '\0';
        strcpy(request.path, slash_pos);
    } else {
        // Hanya hostname, path default "/"
        strcpy(request.host, url);
    }

    return request;
}

void free_request_header(RequestHeader *request) {
    free(request->protocol);
    free(request->host);
    free(request->path);
    free(request->http_version);
    free(request->request_time);
    free(request->encrypted);
}

ResponseHeaders parse_response_headers(const char *response) {
    ResponseHeaders headers = {NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL}; // Inisialisasi struct dengan NULL
    const char *line = response;

    // Memproses setiap baris
    while (line && *line) {
        const char *next_line = strstr(line, "\r\n");
        size_t line_length = next_line ? (size_t)(next_line - line) : strlen(line);

        if (line_length > 0) {
            char *line_copy = strndup(line, line_length);

            if (strncmp(line_copy, "HTTP/", 5) == 0) {
                // Baris pertama, ambil HTTP version dan status code
                char *version = strtok(line_copy, " ");
                char *status = strtok(NULL, "");
                if (version) headers.http_version = strdup(version);
                if (status) headers.status_code = strdup(status);
            } else if (strncmp(line_copy, "Content-Type: ", 14) == 0) {
                headers.content_type = strdup(line_copy + 14);
            } else if (strncmp(line_copy, "Content-Length: ", 16) == 0) {
                headers.content_length = atoi(line_copy + 16);
            } else if (strncmp(line_copy, "Connection: ", 12) == 0) {
                headers.connection = strdup(line_copy + 12);
            } else if (strncmp(line_copy, "Cache-Control: ", 16) == 0) {
                headers.cache_control = strdup(line_copy + 16);
            } else if (strncmp(line_copy, "Encrypted: ", 11) == 0) {
                headers.encrypted = strdup(line_copy + 11);
            } else if (strncmp(line_copy, "Response-Time: ", 15) == 0) {
                headers.response_time = strdup(line_copy + 15);
            }

            free(line_copy);
        }

        line = next_line ? next_line + 2 : NULL; // Pindah ke baris berikutnya
    }

    return headers;
}

// Fungsi untuk membersihkan memori
void free_response_headers(ResponseHeaders *headers) {
    free(headers->http_version);
    free(headers->content_type);
    free(headers->connection);
    free(headers->cache_control);
    free(headers->encrypted);
    free(headers->response_time);
}

char *generate_http_request(RequestHeader *request, const char *method, const char *form_data, int form_data_length) {
    char *request_buffer = (char *)malloc(3 * BUFFER_SIZE);
    if (request_buffer == NULL) {
        return "<h1>Error: Gagal mengalokasikan buffer request!</h1>";
    }

    // Menyusun request sesuai dengan metode HTTP yang dipilih (GET/POST)
    snprintf(request_buffer, 3 * BUFFER_SIZE,
        "%s %s %s/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: EkoBrowser/1.0\r\n"
        "Request-Time: %s\r\n"
        "Encrypted: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n%s\r\n", 
        method, request->path, 
        request->protocol, 
        request->host, 
        request->request_time,
        request->encrypted,
        form_data_length,
        form_data);

    return request_buffer;
}

char *get_time_string() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // Mengambil waktu dalam format struct tm (GMT)
    struct tm *tm_info = localtime(&tv.tv_sec);

    // Alokasikan buffer yang cukup besar untuk waktu dan milidetik
    char *buf = (char *)malloc(64);  // Ukuran buffer yang memadai
    if (!buf) return NULL;  // Cek jika malloc gagal

    // Format waktu tanpa milidetik terlebih dahulu
    strftime(buf, 64, "%a, %d %b %Y %H:%M:%S", tm_info);
    
    // Tambahkan milidetik ke string
    int millis = tv.tv_usec / 1000;
    snprintf(buf + strlen(buf), 64 - strlen(buf), ".%03d GMT", millis);

    return buf;
} //end get_time_string

char *send_request(char *url, char *form_data, char **response_buffer, long *total_bytes_received) {
    int sock_client, response;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[3 * BUFFER_SIZE] = {0};
    long response_buffer_size = 8 * BUFFER_SIZE;

    // Parse URL dengan handle_url
    RequestHeader request = handle_url(url);

    //printf("Hostname : %s\n", request.host);
    if ((server = gethostbyname(request.host)) == NULL) {
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Hostname tidak valid!</h1>";
    }

    if ((sock_client = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Inisialisasi socket client gagal!</h1>";
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(request.port);  // Port default HTTP
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock_client, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock_client);
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Koneksi ke server gagal!</h1>";
    }

    const char *method;
    char *final_form_data; 
    char *requestTime = get_time_string();
    int form_data_length = 0;
    if (form_data == NULL || strcmp(form_data, "") == 0) {
        method = "GET";
        request.encrypted = strdup("no");
        final_form_data = strdup(form_data);
    } else {
        method = "POST";
        request.encrypted = strdup("yes");
        char *key = masehi2jawa(requestTime);
        char *hash_key = sha256_hash(key);
        form_data_length = strlen(form_data);
        final_form_data = encrypt(form_data, hash_key, form_data_length);
    }
    
    request.request_time = requestTime;
    char *request_buffer = generate_http_request(&request, method, final_form_data, form_data_length);  

    if (request_buffer == NULL) {
        close(sock_client);
        free(request_buffer);
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Gagal membangun request HTTP!</h1>";
    }

    printf("\n\nRequest to server :\n %s\n", request_buffer);
    if (send(sock_client, request_buffer, strlen(request_buffer), 0) < 0) {
        close(sock_client);
        free(request_buffer);
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Gagal mengirim permintaan ke server!</h1>";
    }

    *response_buffer = (char *)malloc(response_buffer_size);
    if (*response_buffer == NULL) {
        close(sock_client);
        free(request_buffer);
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Malloc response buffer gagal!</h1>";
    }

    memset(*response_buffer, 0, response_buffer_size);

    while ((response = read(sock_client, buffer, BUFFER_SIZE)) > 0) {
        while (*total_bytes_received + response >= response_buffer_size) {
            response_buffer_size *= 2;
            *response_buffer = realloc(*response_buffer, response_buffer_size);
            if (*response_buffer == NULL) {
                close(sock_client);
                free_request_header(&request);  // Pastikan membebaskan memori sebelum return
                return "<h1>Error : Realloc response buffer gagal!</h1>";
            }
        }

        memcpy(*response_buffer + *total_bytes_received, buffer, response);
        *total_bytes_received += response;
        memset(buffer, 0, BUFFER_SIZE);
    }

    if (response < 0) {
        close(sock_client);
        free(request_buffer);
        free_request_header(&request);  // Pastikan membebaskan memori sebelum return
        return "<h1>Error : Gagal menerima respon server!</h1>";
    }

    close(sock_client);
    free(request_buffer);
    free_request_header(&request);  // Membebaskan memori setelah selesai
    return NULL; // No errors
}

char *handle_response(char *url, char *form_data) {
    char *response_buffer = NULL;
    long total_bytes_received = 0;
    char *body = NULL;

    char *error_message = send_request(url, form_data, &response_buffer, &total_bytes_received);
    if (error_message != NULL) {
        return strdup(error_message);
    }

    char *separator_body = strstr(response_buffer, "\r\n\r\n");
    if (separator_body != NULL) {
        int header_length = separator_body - response_buffer + 4;
        body = response_buffer + header_length;

        ResponseHeaders headers = parse_response_headers(response_buffer);

        // Untuk keperluan trace program.
        // Jika diterapkan pada lingkungan sebenarnya, printf dihapus!
        printf("Response from server :\n%s\n", body);
        if (headers.encrypted != NULL && strcmp(headers.encrypted, "yes") == 0) {
            char *key = masehi2jawa(headers.response_time);
            char *hash_key = sha256_hash(key);

            printf("\nTimes Value : %s\n", headers.response_time);
            printf("Javanese times key : %s\n", key);
            printf("Hash key : %s\n", hash_key);

            char *decrypt_body = decrypt(body, hash_key, headers.content_length);
            if (decrypt_body != NULL) {
                strcpy(body, decrypt_body);
                free(decrypt_body);
            } else {
                body = "<h1>Error : Proses Dekrip GAGAL!</h1>";
            }

            free(key);
            free(hash_key);
        }

        free_response_headers(&headers);
    }

    char *final_body = strdup(body);
    free(response_buffer);
    return final_body;
}
