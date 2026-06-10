#include "ds18b20.h"
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

// =====================================================================
// Configurações e Definições Privadas
// =====================================================================
#define SENSOR_CHIP_PATH "/dev/gpiochip1"
#define LINHA_SENSOR 5
#define MOSFET_CHIP_PATH "/dev/gpiochip1"
#define MOSFET_SENSOR 4

#define CMD_SKIP_ROM        0xCC
#define CMD_CONVERT_T       0x44
#define CMD_READ_SCRATCHPAD 0xBE

// Variáveis de estado (Visíveis apenas neste arquivo)
static struct gpiod_chip *chip_sensor = NULL;
static struct gpiod_line_request *request_sensor = NULL;
static struct gpiod_chip *chip_mosfet = NULL;
static struct gpiod_line_request *request_mosfet = NULL;

// =====================================================================
// Funções Auxiliares Privadas (Camada de Enlace 1-Wire)
// =====================================================================

static void sleep_us(long us) {
    struct timespec start, current; // REMOVIDO o static perigoso
    clock_gettime(CLOCK_MONOTONIC, &start);
    long elapsed = 0;
    long target_ns = us * 1000L;

    while (elapsed < target_ns) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        elapsed = ((current.tv_sec - start.tv_sec) * 1000000000L) + (current.tv_nsec - start.tv_nsec);
    }
}

static void onewire_escrever_bit(int bit) {

    gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 0); 

    if (bit) {
        gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 1); 
        sleep_us(55); 
    } else {
        sleep_us(60); 
        gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 1); 
    }
    sleep_us(2); 
}

static void onewire_escrever_byte(uint8_t dado) {
    for (int i = 0; i < 8; i++) {
        onewire_escrever_bit(dado & 0x01); 
        dado >>= 1;
    }
}

static int onewire_ler_bit(void) {
    int bit = 0;
    
    gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 0); 

    gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 1); 

    bit = gpiod_line_request_get_value(request_sensor, LINHA_SENSOR);
    
    sleep_us(50); 
    return bit;
}

static uint8_t onewire_ler_byte(void) {
    uint8_t dado = 0;
    for (int i = 0; i < 8; i++) {
        if (onewire_ler_bit()) {
            dado |= (1 << i); 
        }
    }
    return dado;
}

static int onewire_reset(void) {
    int presence = 0;
    
    gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 0); // Linha em zero
    sleep_us(480); // Tempo padrão de reset
    
    gpiod_line_request_set_value(request_sensor, LINHA_SENSOR, 1); // Libera a linha

    sleep_us(45); 
    
    presence = gpiod_line_request_get_value(request_sensor, LINHA_SENSOR);
    
    sleep_us(420); // Tempo de recuperação do barramento
    return (presence == 0) ? 1 : 0; 
}

// =====================================================================
// Implementação da API Pública
// =====================================================================

int ds18b20_inicializar_gpios(void) {
    unsigned int offset_sensor = LINHA_SENSOR;
    unsigned int offset_mosfet = MOSFET_SENSOR;

    chip_sensor = gpiod_chip_open(SENSOR_CHIP_PATH);
    if (!chip_sensor) { return -1; }

    chip_mosfet = gpiod_chip_open(MOSFET_CHIP_PATH);
    if (!chip_mosfet) { return -1; }

    // Configuração Sensor (Open-Drain)
    struct gpiod_line_settings *cfg_sensor = gpiod_line_settings_new();
    struct gpiod_line_config *line_cfg_sensor = gpiod_line_config_new();
    struct gpiod_request_config *req_cfg_sensor = gpiod_request_config_new();

    gpiod_line_settings_set_direction(cfg_sensor, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_drive(cfg_sensor, GPIOD_LINE_DRIVE_OPEN_DRAIN);
    gpiod_line_settings_set_output_value(cfg_sensor, GPIOD_LINE_VALUE_ACTIVE); 
    gpiod_line_config_add_line_settings(line_cfg_sensor, &offset_sensor, 1, cfg_sensor);
    gpiod_request_config_set_consumer(req_cfg_sensor, "ds18b20_dq");

    request_sensor = gpiod_chip_request_lines(chip_sensor, req_cfg_sensor, line_cfg_sensor);

    // Configuração MOSFET (Push-Pull)
    struct gpiod_line_settings *cfg_mosfet = gpiod_line_settings_new();
    struct gpiod_line_config *line_cfg_mosfet = gpiod_line_config_new();
    struct gpiod_request_config *req_cfg_mosfet = gpiod_request_config_new();

    gpiod_line_settings_set_direction(cfg_mosfet, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_drive(cfg_mosfet, GPIOD_LINE_DRIVE_PUSH_PULL);
    gpiod_line_settings_set_output_value(cfg_mosfet, GPIOD_LINE_VALUE_ACTIVE);
    gpiod_line_config_add_line_settings(line_cfg_mosfet, &offset_mosfet, 1, cfg_mosfet);
    gpiod_request_config_set_consumer(req_cfg_mosfet, "mosfet_gate");

    request_mosfet = gpiod_chip_request_lines(chip_mosfet, req_cfg_mosfet, line_cfg_mosfet);

    // Limpeza de memória das configurações
    gpiod_line_settings_free(cfg_sensor);
    gpiod_line_config_free(line_cfg_sensor);
    gpiod_request_config_free(req_cfg_sensor);
    gpiod_line_settings_free(cfg_mosfet);
    gpiod_line_config_free(line_cfg_mosfet);
    gpiod_request_config_free(req_cfg_mosfet);

    if (!request_sensor || !request_mosfet) {
        return -1; 
    }
    return 0;
}

void ds18b20_executar_fase_A(void) {
    if (!onewire_reset()) {
        printf("[DS18B20] Fase A: Falha - Sensor ausente.\n");
        return;
    }
    onewire_escrever_byte(CMD_SKIP_ROM);
    onewire_escrever_byte(CMD_CONVERT_T);

    // Ativa o Strong Pull-up (Drena o Gate do canal P para 0V para ligar o transistor)
    gpiod_line_request_set_value(request_mosfet, MOSFET_SENSOR, 0);
    usleep(750000); // Mantém alimentado por 750ms estáveis via usleep comum
    gpiod_line_request_set_value(request_mosfet, MOSFET_SENSOR, 1); // Desliga o MOSFET (3.3V)
}

float ds18b20_executar_fase_B(void) {
    uint8_t temp_lsb, temp_msb;
    int16_t temperatura_raw;

    if (!onewire_reset()) {
        printf("[DS18B20] Fase B: Falha - Pulso presence ausente.\n");
        return -1000.0;
    }

    onewire_escrever_byte(CMD_SKIP_ROM);
    onewire_escrever_byte(CMD_READ_SCRATCHPAD);

    temp_lsb = onewire_ler_byte(); 
    temp_msb = onewire_ler_byte(); 

    temperatura_raw = (temp_msb << 8) | temp_lsb;
    return (float)temperatura_raw * 0.0625f;
}

void ds18b20_liberar_hardware(void) {
    if (request_sensor) gpiod_line_request_release(request_sensor);
    if (chip_sensor) gpiod_chip_close(chip_sensor);
    if (request_mosfet) gpiod_line_request_release(request_mosfet);
    if (chip_mosfet) gpiod_chip_close(chip_mosfet);
}