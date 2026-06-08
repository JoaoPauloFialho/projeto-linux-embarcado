#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h> // Adicionado para suportar threads
#include <gpiod.h>   // Biblioteca nativa para leitura de GPIO

// =================================================================
// CONFIGURAÇÕES DE HARDWARE
// =================================================================

// Configurações do PWM (LED no P8_13 / sysfs)
#define PWM_CHIP_DIR "/sys/class/pwm/pwmchip0"
#define PWM_CHANNEL "1"
#define PERIOD_NS 1000000 

// Configurações do Botão (libgpiod)
#define BOTAO_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BOTAO 2

// Configurações do Buzzer (libgpiod)
#define BUZZER_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BUZZER 3

// Configurações de tempo (em microssegundos para o usleep)
#define TEMPO_RAPIDO_US 100000 
#define TEMPO_LENTO_US 500000  

// =================================================================
// VARIÁVEIS GLOBAIS
// =================================================================
char pwm_dir[256];

struct gpiod_chip *chip = NULL;
struct gpiod_line_request *request = NULL;

struct gpiod_chip *chip_buzzer = NULL;
struct gpiod_line_request *request_buzzer = NULL;

int alarme_ativo = 0; 

void write_sysfs(const char *path, const char *filename, const char *value) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", path, filename);
    
    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        perror("Erro ao abrir o arquivo do sysfs");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%s", value);
    fclose(fp);
}

void rotina_de_limpeza(int sinal) {
    printf("\n[Sinal recebido] Desligando hardware e liberando os pinos...\n");
    
    // 1. Desliga o PWM
    write_sysfs(pwm_dir, "duty_cycle", "0");
    write_sysfs(pwm_dir, "enable", "0");
    
    // 2. Libera os recursos do Botão
    if (request) gpiod_line_request_release(request);
    if (chip) gpiod_chip_close(chip);

    // 3. Libera os recursos do Buzzer
    if (request_buzzer) gpiod_line_request_release(request_buzzer);
    if (chip_buzzer) gpiod_chip_close(chip_buzzer);

    exit(EXIT_SUCCESS);
}

// =================================================================
// THREAD DO BUZZER (RODA EM PARALELO)
// =================================================================
void *thread_buzzer_func(void *arg) {
    unsigned int offset_buzzer = LINHA_BUZZER;

    chip_buzzer = gpiod_chip_open(BUZZER_CHIP_PATH);
    if (!chip_buzzer) pthread_exit(NULL);

    struct gpiod_line_settings *config_buzzer = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(config_buzzer, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(config_buzzer, 0); // Inicia desligado

    struct gpiod_line_config *line_cfg_buzzer = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg_buzzer, &offset_buzzer, 1, config_buzzer);

    struct gpiod_request_config *req_cfg_