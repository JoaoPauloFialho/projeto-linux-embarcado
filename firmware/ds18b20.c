/**
 * @file ds18b20.c
 * @brief Implementação dos métodos de leitura do DS18B20 baseada em sysfs.
 * @author Eduardo Silva de Oliveira
 * @date 2026-06-12
 */

#include "ds18b20.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/** @brief Caminho padrão do barramento 1-Wire no Linux. */
#define W1_BUS_PATH "/sys/bus/w1/devices"

/** @brief Armazena o caminho absoluto do arquivo w1_slave do sensor localizado. */
static char sensor_path[256] = {0};

int ds18b20_inicializar(void) {
    DIR *dir;
    struct dirent *dirent;
    int sensor_encontrado = 0;

    dir = opendir(W1_BUS_PATH);
    if (dir == NULL) {
        perror("[DS18B20] Erro ao abrir o diretorio do barramento");
        return -1;
    }

    /* Procura por diretórios que iniciam com o ID da família (28-) */
    while ((dirent = readdir(dir)) != NULL) {
        if (strncmp(dirent->d_name, "28-", 3) == 0) {
            snprintf(sensor_path, sizeof(sensor_path), "%s/%s/w1_slave", W1_BUS_PATH, dirent->d_name);
            sensor_encontrado = 1;
            break;
        }
    }
    closedir(dir);

    if (!sensor_encontrado) {
        printf("[DS18B20] Nenhum dispositivo detectado no barramento.\n");
        return -1;
    }

    printf("[DS18B20] Dispositivo validado em: %s\n", sensor_path);
    return 0;
}

float ds18b20_ler_temperatura(void) {
    FILE *fp;
    char buffer[256];
    char *pos_t;
    float temperatura = -1000.0f;
    int crc_valido = 0;

    if (sensor_path[0] == '\0') {
        return temperatura;
    }

    fp = fopen(sensor_path, "r");
    if (fp == NULL) {
        return temperatura;
    }

    /* Linha 1: Validação do checksum (CRC) */
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, "YES") != NULL) {
            crc_valido = 1;
        }
    }

    /* Linha 2: Extração e conversão do valor numérico */
    if (crc_valido && fgets(buffer, sizeof(buffer), fp) != NULL) {
        pos_t = strstr(buffer, "t=");
        if (pos_t != NULL) {
            long temp_raw = atol(pos_t + 2);
            temperatura = (float)temp_raw / 1000.0f;
        }
    }

    fclose(fp);
    return temperatura;
}

void ds18b20_liberar_hardware(void) {
    sensor_path[0] = '\0';
}