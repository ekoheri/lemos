#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../include/libutils.h"

void bytes_to_hex(const uint8_t* bytes, char* hex, size_t bytes_len) {
    for (size_t i = 0; i < bytes_len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[bytes_len * 2] = '\0';
}

void hex_to_bytes(const char* hex, uint8_t* bytes, size_t bytes_len) {
    for (size_t i = 0; i < bytes_len; i++) {
        sscanf(hex + 2 * i, "%2hhx", &bytes[i]);
    }
}

char *base64_encode(const unsigned char *data, size_t input_length) {
    const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);  // Estimasi panjang output base64
    char *encoded_data = (char *)malloc(output_length + 1);
    if (encoded_data == NULL) {
        fprintf(stderr, "Error allocating memory for base64 encoding\n");
        exit(EXIT_FAILURE);
    }

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_table[triple & 0x3F];
    }

    // Menambahkan padding jika perlu
    for (size_t i = 0; i < (output_length - j); i++) {
        encoded_data[output_length - 1 - i] = '=';
    }
    encoded_data[output_length] = '\0'; // Null terminator
    return encoded_data;
}