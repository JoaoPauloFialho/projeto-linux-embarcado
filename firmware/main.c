#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> 
#include <gpiod.h>   

// =================================================================
// CONFIGURAÇÕES DE HARDWARE
// =================================================================

// Configurações do Botão
#define BOTAO_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BOTAO 2

// Configurações do Buzzer
#define BUZZER_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BUZZER 3

// =================================================================
// VARIÁVEIS GLOBAIS
// =================================================================

// Recursos do botão
struct gpiod_chip *chip_botao = NULL;
struct gpiod_line_request *request_botao = NULL;

// Recursos do buzzer
struct gpiod_chip *chip_buzzer = NULL;
struct gpiod_line_request *request_buzzer = NULL;

// 'volatile' obriga a thread a ler o valor real da memória sempre
volatile int alarme_ativo = 0; 

// =================================================================
// ROTINA DE LIMPEZA (CTRL+C)
// =================================================================
void rotina_de_limpeza(int sinal) {
    printf("\n[Sinal recebido] Desligando hardware e liberando os pinos...\n");
    
    // Libera os recursos do Botão
    if (request_botao) gpiod_line_request_release(request_botao);
    if (chip_botao) gpiod_chip_close(chip_botao);

    // Libera os recursos do Buzzer
    if (request_buzzer) gpiod_line_request_release(request_buzzer);
    if (chip_buzzer) gpiod_chip_close(chip_buzzer);

    exit(EXIT_SUCCESS);
}

// =================================================================
// THREAD DO BUZZER (RODA EM PARALELO)
// =================================================================
void *thread_buzzer_func(void *arg) {
    unsigned int offset_buzzer = LINHA_BUZZER;

    // Inicializa o chip e o pino do buzzer como SAÍDA
    chip_buzzer = gpiod_chip_open(BUZZER_CHIP_PATH);
    if (!chip_buzzer) pthread_exit(NULL);

    struct gpiod_line_settings *config_buzzer = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(config_buzzer, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(config_buzzer, 0); // Inicia desligado

    struct gpiod_line_config *line_cfg_buzzer = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg_buzzer, &offset_buzzer, 1, config_buzzer);

    struct gpiod_request_config *req_cfg_buzzer = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg_buzzer, "saida_buzzer");

    request_buzzer = gpiod_chip_request_lines(chip_buzzer, req_cfg_buzzer, line_cfg_buzzer);
    
    gpiod_request_config_free(req_cfg_buzzer);
    gpiod_line_config_free(line_cfg_buzzer);
    gpiod_line_settings_free(config_buzzer);

    // Loop infinito da thread verificando a variável global
    while (1) {
        printf("[Thread Buzzer] Alarme ativo: %d\n", alarme_ativo);
        if (alarme_ativo == 1) {
            gpiod_line_request_set_value(request_buzzer, offset_buzzer, 1);
            usleep(200000); 
            gpiod_line_request_set_value(request_buzzer, offset_buzzer, 0);
            usleep(200000); 
        } else {
            gpiod_line_request_set_value(request_buzzer, offset_buzzer, 0);
            usleep(100000); // Pausa leve para não sobrecarregar a CPU
        }
    }
    return NULL;
}

// =================================================================
// FUNÇÃO PRINCIPAL
// =================================================================
int main(void) {
    unsigned int offset_botao = LINHA_BOTAO;

    signal(SIGINT, rotina_de_limpeza);

    // 1. INICIA A THREAD DO BUZZER
    pthread_t thread_buzzer;
    pthread_create(&thread_buzzer, NULL, thread_buzzer_func, NULL);

    // 2. CONFIGURA O BOTÃO COMO ENTRADA
    chip_botao = gpiod_chip_open(BOTAO_CHIP_PATH);
    struct gpiod_line_settings *config_botao = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(config_botao, GPIOD_LINE_DIRECTION_INPUT);
    
    struct gpiod_line_config *line_cfg_botao = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg_botao, &offset_botao, 1, config_botao);
    
    struct gpiod_request_config *req_cfg_botao = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg_botao, "leitura_botao");
    
    request_botao = gpiod_chip_request_lines(chip_botao, req_cfg_botao, line_cfg_botao);

    gpiod_request_config_free(req_cfg_botao);
    gpiod_line_config_free(line_cfg_botao);
    gpiod_line_settings_free(config_botao);

    printf("Sistema iniciado! Testando thread em ciclo de 2 segundos...\n");

    // 3. LOOP PRINCIPAL
    for(;;) {
        // Alterna o estado do alarme
        alarme_ativo = !alarme_ativo;
        
        // Lê o estado do botão apenas para imprimir na tela se está sendo pressionado
        int estado_botao = gpiod_line_request_get_value(request_botao, offset_botao);

        printf("Alarme ativo: %d | Botão Pressionado: %s\n", alarme_ativo, estado_botao == 0 ? "SIM" : "NAO");

        // Pausa a thread principal por 2 segundos
        sleep(2); 
    }
    
    return EXIT_SUCCESS;
}