#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "../include/config.h"

// Inisialisasi variabel global config
Config config = {0};

char *trim(char *str) {
    char *end;

    // Hapus spasi di depan
    while (isspace((unsigned char)*str)) {
        str++;
    }

    // Jika hanya ada spasi
    if (*str == 0) {
        return str;
    }

    // Hapus spasi di belakang
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    // Tambahkan null-terminator setelah karakter terakhir yang bukan spasi
    *(end + 1) = '\0';

    return str;
}

// Fungsi untuk membaca file konfigurasi
void load_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening config file");
        return;
    }

    char line[1024];  // Buffer untuk membaca setiap baris
    char section[256];  // Menyimpan nama section (misalnya [Performance])

    while (fgets(line, sizeof(line), file)) {
        // Menghapus karakter newline di akhir baris
        line[strcspn(line, "\n")] = '\0';

        // Lewati komentar
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        // Menangani bagian [Performance] (atau bagian lainnya)
        if (line[0] == '[') {
            sscanf(line, "[%255[^]]]", section);  // Menyimpan nama section
            continue;
        }

        // Mencari tanda "=" yang memisahkan key dan value
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (key != NULL && value != NULL) {
            // Hapus spasi di sekitar key dan value
            key = trim(key);
            value = trim(value);

            // Menangani key tertentu
            if (strcmp(key, "encrypt_directory") == 0) {
                strncpy(config.encrypt_directory, value, sizeof(config.encrypt_directory));
            } else if (strcmp(key, "encryption_type") == 0) {
                strncpy(config.encryption_type, value, sizeof(config.encryption_type));
            }
        }
    }

    fclose(file);
}
