#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> 
#include <gpiod.h>   
#include <sys/inotify.h>
#include <fcntl.h>
#include "oled_i2c.h" // Biblioteca do display OLED

// Configurações do Botão
#define BOTAO_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BOTAO 2

// Configurações do Buzzer
#define BUZZER_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BUZZER 3

// Configuração do Inotify
#define ALARME_CONF_PATH "/home/edujoao/projeto-linux-embarcado/firmware/alarme.conf"

// Configuração do Barramento I2C (OLED)
#define I2C_BUS "/dev/i2c-2" 
#define I2C_ADDR 0x3C 

struct gpiod_chip *chip_botao = NULL;
struct gpiod_line_request *request_botao = NULL;
struct gpiod_chip *chip_buzzer = NULL;
struct gpiod_line_request *request_buzzer = NULL;

// Variáveis globais de controle
volatile int alarme_ativo = 0; 
volatile float limite_temp = 0.0;
volatile float temp_atual = 30.0; // Valor simulado para testes

void rotina_de_limpeza(int sinal) {
    printf("\n[Sinal recebido] Desligando hardware e liberando os pinos...\n");
    
    // Libera os recursos do Botão
    if (request_botao) gpiod_line_request_release(request_botao);
    if (chip_botao) gpiod_chip_close(chip_botao);

    // Libera os recursos do Buzzer
    if (request_buzzer) gpiod_line_request_release(request_buzzer);
    if (chip_buzzer) gpiod_chip_close(chip_buzzer);

    close_oled();

    exit(EXIT_SUCCESS);
}

// Função para ler o arquivo e gravar na variável global
void atualizar_limite_temp() {
    FILE *file = fopen(ALARME_CONF_PATH, "r");
    if (file) {
        float novo_limite;
        // Lê o arquivo e tenta converter para float
        if (fscanf(file, "%f", &novo_limite) == 1) {
            limite_temp = novo_limite;
            printf("[Inotify] alarme.conf alterado! Novo limite de temperatura: %.2f°C\n", limite_temp);
        }
        fclose(file);
    } else {
        perror("[Inotify] Erro ao abrir o arquivo alarme.conf para leitura");
    }
}

// Thread dedicada para monitorar o arquivo sem bloquear o resto do programa
void *thread_inotify_func(void *arg) {
    int fd, wd;
    // Buffer alinhado necessário para estruturar os eventos do inotify
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    fd = inotify_init();
    if (fd < 0) {
        perror("[Inotify] Falha ao inicializar");
        pthread_exit(NULL);
    }

    // Monitora a modificação do arquivo (IN_MODIFY)
    wd = inotify_add_watch(fd, ALARME_CONF_PATH, IN_MODIFY);
    if (wd < 0) {
        printf("[Inotify] Aviso: Não foi possível monitorar %s. Verifique se o arquivo existe.\n", ALARME_CONF_PATH);
    }

    // Lê o valor atual assim que a thread inicia
    atualizar_limite_temp();

    // Loop infinito aguardando modificações
    for(;;) {
        // A função read() fica bloqueada aqui, consumindo 0% de CPU, até o arquivo mudar
        int length = read(fd, buffer, sizeof(buffer));
        if (length > 0) {
            // Dispara a leitura se o evento ocorreu
            atualizar_limite_temp();
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    return NULL;
}

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
    for (;;) {
        if (temp_atual >= limite_temp) {
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

int main(void) {
    unsigned int offset_botao = LINHA_BOTAO;

    signal(SIGINT, rotina_de_limpeza);

    // INICIALIZA O DISPLAY OLED
    if (init_oled(I2C_BUS, I2C_ADDR) == 0) {
        printf("Display OLED inicializado com sucesso.\n");
        ssd1306_set_cursor(0, 0); 
        oled_print("SISTEMA INICIADO");
    } else {
        printf("Falha na inicializacao do OLED.\n");
    }

    // 1. INICIA A THREAD DO INOTIFY (MONITORAMENTO DO ARQUIVO)
    pthread_t thread_inotify;
    pthread_create(&thread_inotify, NULL, thread_inotify_func, NULL);

    // 2. INICIA A THREAD DO BUZZER
    pthread_t thread_buzzer;
    pthread_create(&thread_buzzer, NULL, thread_buzzer_func, NULL);

    // 3. CONFIGURA O BOTÃO COMO ENTRADA
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

    char linha1[32];
    char linha2[32];

    printf("Sistema iniciado! Testando threads...\n");
    for(;;) {
        int estado_botao = gpiod_line_request_get_value(request_botao, offset_botao);
        printf("Alarme ativo: %d | Botão Pressionado: %s | Limite Temp: %.2f°C\n", 
               alarme_ativo, estado_botao == 0 ? "SIM" : "NAO", limite_temp);

        // Atualiza as strings do display
        snprintf(linha1, sizeof(linha1), "Temp Atual: %.1f", temp_atual);
        snprintf(linha2, sizeof(linha2), "Limite: %.1f", limite_temp);

        // Imprime no display OLED
        ssd1306_set_cursor(0, 0); 
        oled_print(linha1);
        ssd1306_set_cursor(0, 2); 
        oled_print(linha2);

        sleep(2); 
    }
    
    return EXIT_SUCCESS;
}