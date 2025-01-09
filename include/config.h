// config.h

#ifndef CONFIG_H
#define CONFIG_H

// Struktur data untuk konfigurasi
typedef struct {
    char encrypt_directory[256];
    char encryption_type[64];
} Config;

// Deklarasikan variabel global untuk menyimpan konfigurasi
extern Config config;

//Fungsi untuk membersihkan spasi diawal dan diakhir string
char *trim(char *str);

// Fungsi untuk memuat konfigurasi dari file
void load_config(const char *filename);

#endif
