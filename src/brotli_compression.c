#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <brotli/encode.h>
#include <brotli/decode.h>

#include "../include/libutils.h"
#include "../include/brotli_compression.h"

#define BUFFER_SIZE 1024 * 1024

// Fungsi kompresi Brotli
char *compress_brotli(const char *input, size_t input_size) {
    size_t max_output_size = BrotliEncoderMaxCompressedSize(input_size);
    uint8_t *compressed = malloc(max_output_size);
    if (!compressed) return NULL;

    if (!BrotliEncoderCompress(
            BROTLI_DEFAULT_QUALITY,
            BROTLI_DEFAULT_WINDOW,
            BROTLI_MODE_TEXT,
            input_size,
            (const uint8_t *)input,
            &max_output_size,
            compressed)) {
        free(compressed);
        return NULL;
    }

    char *b64 = base64_encode(compressed, max_output_size);
    free(compressed);
    return b64;
}

uint8_t hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0xFF;  // invalid
}

void hex_to_byte(const char* hex, uint8_t* bytes, size_t bytes_len) {
    for (size_t i = 0; i < bytes_len; ++i) {
        uint8_t high = hex_char_to_val(hex[i * 2]);
        uint8_t low = hex_char_to_val(hex[i * 2 + 1]);

        if (high == 0xFF || low == 0xFF) {
            // Invalid hex char, fallback to zero or handle error
            bytes[i] = 0;
        } else {
            bytes[i] = (high << 4) | low;
        }
    }
}

void debug_print_hex(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

char *decompress_brotli(const char *input_base64, size_t *output_size) {
    // Langkah 1: Decode base64 → hasil: string hex
    char *hexstr = base64_decode(input_base64);  // hasil: "ABCDEF..."
    if (!hexstr) return NULL;

    size_t hex_len = strlen(hexstr);
    if (hex_len % 2 != 0) {
        free(hexstr);
        return NULL;  // panjang hex harus genap
    }

    // Langkah 2: Convert hex string → binary
    size_t compressed_size = hex_len / 2;
    uint8_t *compressed = malloc(compressed_size);
    if (!compressed) {
        free(hexstr);
        return NULL;
    }
    hex_to_bytes(hexstr, compressed, compressed_size);
    free(hexstr);  // tidak dibutuhkan lagi

    // Langkah 3: Brotli decompress
    size_t output_buf_size = compressed_size * 2;  // Estimasi yang lebih aman
    uint8_t *output = malloc(output_buf_size);
    if (!output) {
        free(compressed);
        return NULL;
    }

    BrotliDecoderResult result = BrotliDecoderDecompress(
        compressed_size,
        compressed,
        &output_buf_size,
        output
    );
    free(compressed);

    if (result != BROTLI_DECODER_RESULT_SUCCESS) {
        fprintf(stderr, "Brotli decompression failed: %d\n", result);
        free(output);
        return NULL;
    }

    // Langkah 4: Ubah ke string biasa
    char *output_str = malloc(output_buf_size + 1);
    if (!output_str) {
        free(output);
        return NULL;
    }

    // Hanya menyalin ukuran yang benar sesuai dengan hasil dekompresi
    memcpy(output_str, output, output_buf_size);
    output_str[output_buf_size] = '\0';  // null-terminate

    free(output);

    // Menyimpan ukuran output, jika diminta
    if (output_size) {
        *output_size = output_buf_size;
    }

    return output_str;
}

