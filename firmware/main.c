#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> 
#include <gpiod.h>
#include "ds18b20.h"
#include <time.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include "oled_i2c.h"
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

// Configurações do Botão
#define BOTAO_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BOTAO 2

// Configurações do Buzzer
#define BUZZER_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BUZZER 3

//Nome do arquivo de temperatuda
#define ARQUIVO_LOG "Log_temperatura.csv"

// Configuração do Inotify
#define ALARME_CONF_PATH "/home/edujoao/projeto-linux-embarcado/firmware/alarme.conf"

// =================================================================
// VARIÁVEIS GLOBAIS
// =================================================================

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

// Variáveis globais para o controle de alternância da tela OLED
volatile int tela_atual = 1; 
volatile int limpar_tela = 0; 
float sd_livre_mock = 14.5; 

void rotina_de_limpeza(int sinal) {
    printf("\n[Sinal recebido] Desligando hardware e liberando os pinos...\n");
    
    // Libera os recursos do Botão
    if (request_botao) gpiod_line_request_release(request_botao);
    if (chip_botao) gpiod_chip_close(chip_botao);

    // Libera os recursos do Buzzer
    if (request_buzzer) gpiod_line_request_release(request_buzzer);
    if (chip_buzzer) gpiod_chip_close(chip_buzzer);

    ds18b20_liberar_hardware();

    close_oled();

    exit(EXIT_SUCCESS);
}

// Função para buscar o IP da BeagleBone dinamicamente
void obter_ip_atual(char *buffer_ip) {
    struct ifaddrs *ifaddr, *ifa;
    strcpy(buffer_ip, "Sem rede"); 

    if (getifaddrs(&ifaddr) == -1) return;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (strcmp(ifa->ifa_name, "lo") != 0) { 
                struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
                strcpy(buffer_ip, inet_ntoa(pAddr->sin_addr));
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
}

// =================================================================
// THREAD DO SENSOR e save em csv
// =================================================================

void salvar_em_csv(float temperatura) {
    FILE *arquivo;
    int precisa_cabecalho = 0;

    // 1. Verifica se o arquivo já existe. Se não existir (access retorna -1),
    // marcamos a flag para escrever o cabeçalho do CSV na primeira linha.
    if (access(ARQUIVO_LOG, F_OK) == -1) {
        precisa_cabecalho = 1;
    }

    // 2. Abre o arquivo em modo "append" (anexar). 
    // Se não existir, ele cria. Se existir, ele adiciona ao final.
    arquivo = fopen(ARQUIVO_LOG, "a");
    if (arquivo == NULL) {
        printf("[ERRO] Nao foi possivel abrir/criar o arquivo %s\n", ARQUIVO_LOG);
        return;
    }

    // 3. Escreve o cabeçalho apenas se o arquivo acabou de ser criado
    if (precisa_cabecalho) {
        fprintf(arquivo, "Data,Hora,Temperatura(C)\n");
    }

    // 4. Captura o tempo atual do sistema operacional
    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);

    // 5. Formata a string e grava no arquivo
    // Formato: DD/MM/YYYY,HH:MM:SS,Valor
    fprintf(arquivo, "%02d/%02d/%04d,%02d:%02d:%02d,%.2f\n",
            tm_info.tm_mday, tm_info.tm_mon + 1, tm_info.tm_year + 1900,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
            temperatura);

    // 6. Fecha o arquivo para liberar o buffer e garantir a gravação no disco
    fclose(arquivo);
}

void *thread_sensor_func (void * arg){
    float temp;
    ds18b20_inicializar_gpios();
    while (1){
        ds18b20_executar_fase_A();
        temp = ds18b20_executar_fase_B();
        if(temp == -1000){
            return NULL;
        }
        temp_atual = temp; 
        printf("[Sensor] Temperatura atual: %.2f°C\n", temp);
        salvar_em_csv(temp);
    }
}

// =================================================================
// Buzzer e Inotify
// =================================================================

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

// Thread dedicada exclusivamente para leitura e debounce do botão
void *thread_botao_func(void *arg) {
    unsigned int offset_botao = LINHA_BOTAO;
    int estado_anterior = 1;

    for (;;) {
        int estado_atual = gpiod_line_request_get_value(request_botao, offset_botao);

        if (estado_atual == 0 && estado_anterior == 1) {
            usleep(50000); // Debounce de 50ms
            
            if (gpiod_line_request_get_value(request_botao, offset_botao) == 0) {
                tela_atual = (tela_atual == 1) ? 2 : 1; 
                limpar_tela = 1; 
                
                while (gpiod_line_request_get_value(request_botao, offset_botao) == 0) {
                    usleep(10000);
                }
            }
        }
        estado_anterior = estado_atual;
        usleep(10000); 
    }
    return NULL;
}

int main(void) {
    unsigned int offset_botao = LINHA_BOTAO;

    signal(SIGINT, rotina_de_limpeza);

    // INICIALIZA O DISPLAY OLED
    if (init_oled(I2C_BUS, I2C_ADDR) == 0) {
        printf("Display OLED initialized com sucesso.\n");
        ssd1306_set_cursor(0, 0); 
        oled_print("SISTEMA INICIADO");
    } else {
        printf("Falha na inicializacao do OLED.\n");
    }

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

    // INICIA AS THREADS DE SUPORTE
    pthread_t thread_inotify, thread_buzzer, thread_botao_id, thread_sensor;
    pthread_create(&thread_inotify, NULL, thread_inotify_func, NULL);
    pthread_create(&thread_buzzer, NULL, thread_buzzer_func, NULL);
    pthread_create(&thread_botao_id, NULL, thread_botao_func, NULL);
    pthread_create(&thread_sensor, NULL, thread_sensor_func, NULL);

    char linha1[32];
    char linha2[32];
    char ip_buffer[32];

    printf("Sistema iniciado! Testando threads...\n");
    for(;;) {
        int estado_botao = gpiod_line_request_get_value(request_botao, offset_botao);
        printf("Alarme ativo: %d | Botão Pressionado: %s | Limite Temp: %.2f°C\n", 
               alarme_ativo, estado_botao == 0 ? "SIM" : "NAO", limite_temp);

        // Recebe o sinal da thread do botão para resetar a tela
        if (limpar_tela == 1) {
            ssd1306_clear();
            limpar_tela = 0;
        }

        if (tela_atual == 1) {
            // Atualiza as strings do display
            snprintf(linha1, sizeof(linha1), "Temp Atual: %.1f", temp_atual);
            snprintf(linha2, sizeof(linha2), "Limite: %.1f", limite_temp);
        } else {
            obter_ip_atual(ip_buffer);
            snprintf(linha1, sizeof(linha1), "IP: %s", ip_buffer);
            snprintf(linha2, sizeof(linha2), "SD: %.1f GB free", sd_livre_mock);
        }

        // Imprime no display OLED
        ssd1306_set_cursor(0, 0); 
        oled_print(linha1);
        ssd1306_set_cursor(0, 2); 
        oled_print(linha2);

        sleep(2); 
    }
    
    return EXIT_SUCCESS;
}