#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/external.h"

typedef struct {
    char *data;
    size_t data_size;
    int encrypted;
    char *content_type;  // Menyimpan content_type
} download_context;

size_t write_data(void *ptr, size_t size, size_t nmemb, download_context *context) {
    size_t realsize = size * nmemb;

    // Periksa apakah content_type adalah gambar
    int is_image = (strstr(context->content_type, "image/") != NULL);

    context->data = realloc(context->data, context->data_size + realsize + 1);
    if (context->data == NULL) {
        fprintf(stderr, "Not enough memory!\n");
        return 0;
    }
    memcpy(context->data + context->data_size, ptr, realsize);
    context->data_size += realsize;
    if (!is_image) {
        context->data[context->data_size] = '\0';
    }
    return realsize;
}

size_t header_callback(char *ptr, size_t size, size_t nmemb, download_context *context) {
    size_t realsize = size * nmemb;
    char *header_line = strndup(ptr, realsize);

    // Cek apakah header adalah Content-Type
    if (strstr(header_line, "Content-Type:")) {
        // Ambil nilai setelah "Content-Type: "
        context->content_type = strdup(header_line + strlen("Content-Type: "));
        context->content_type[strcspn(context->content_type, "\r\n")] = '\0';  // Menghapus newline
    } 
    else if (strstr(header_line, "Encrypted:")) {
        if (strstr(header_line, "yes")) {
            context->encrypted = 1;
        }
    }

    free(header_line);
    return realsize;
}

char *download_resource(const char *url) {
    CURL *curl;
    CURLcode res;
    download_context context = {NULL, 0, 0, NULL};

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(context.data);
            context.data = NULL;
        } /*else {
            if (context.encrypted) {
                printf("Response is encrypted.\n");
                char *decrypt_body = chacha20_decrypt(context.data, key, nonce, counter);
                free(context.data);
                context.data = decrypt_body;
            } else {
                printf("Response is not encrypted.\n");
            }
        }*/

        curl_easy_cleanup(curl);
    }
    return context.data;
}

char *resolve_relative_url(const char *base_url, const char *relative_url) {
    xmlChar *full_url = xmlBuildURI((const xmlChar *)relative_url, (const xmlChar *)base_url);
    char *resolved_url = strdup((const char *)full_url);
    xmlFree(full_url);
    return resolved_url;
}

void find_external_resources(const char *html_content, const char *base_url) {
    htmlDocPtr doc = htmlReadMemory(html_content, strlen(html_content), NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (doc == NULL) {
        printf("Failed to parse HTML\n");
        return;
    }
    
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    if (xpathCtx == NULL) {
        printf("Failed to create XPath context\n");
        xmlFreeDoc(doc);
        return;
    }

    const char *xpathExpr = "//img/@src | //link/@href | //script/@src";
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)xpathExpr, xpathCtx);
    if (xpathObj == NULL) {
        printf("Failed to evaluate XPath expression\n");
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return;
    }

    xmlNodeSetPtr nodes = xpathObj->nodesetval;

    // Periksa apakah nodes tidak NULL dan memiliki elemen
    if (nodes == NULL || nodes->nodeNr == 0) {
        //printf("No nodes found\n");
    } else {
        for (int i = 0; i < nodes->nodeNr; i++) {
            xmlNodePtr node = nodes->nodeTab[i];
            if (node) {  // Pastikan node tidak NULL
                char *url = (char *)xmlNodeGetContent(node);
                if (url) { // Pastikan url tidak NULL
                    char *resolved_url = resolve_relative_url(base_url, url);
                    if (resolved_url) { // Pastikan resolved_url tidak NULL
                        char *resource = download_resource(resolved_url);
                        if (resource) {
                            //printf("Fetched resource from: %s\n", resolved_url);
                            free(resource);
                        }
                        free(resolved_url);
                    }
                    xmlFree(url);
                }
            }
        }
    }
    
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
}

