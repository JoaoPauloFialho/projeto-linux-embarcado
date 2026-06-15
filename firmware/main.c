#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> 
#include <gpiod.h>
#include <time.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include "ds18b20.h" // Inserção: Biblioteca para o sensor de temperatura DS18B20
#include "gmt130_spi.h" // Inserção: Nova biblioteca do display SPI

// Configurações do Botão
#define BOTAO_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BOTAO 2

// Pinos SPI do Display configurados via GPIO (Chip 2 corresponde ao barramento de pinos que inclui P8_9 e P8_10)
#define DISPLAY_CHIP_PATH "/dev/gpiochip2"
#define LINHA_RES 5 // p8_9 (GPIO 69 = gpiochip2, linha 5)
#define LINHA_DC 4  // p8_10 (GPIO 68 = gpiochip2, linha 4)
#define SPI_DEVICE "/dev/spidev0.0"

// Configurações do Buzzer
#define BUZZER_CHIP_PATH "/dev/gpiochip1"
#define LINHA_BUZZER 3

//Nome do arquivo de temperatuda
// #define ARQUIVO_LOG "Log_temperatura.csv"

//Nome do arquivo de temperatuda
#define ARQUIVO_LOG "/mnt/sdcard/Log_temperatura.csv"

// Configuração do Inotify
#define ALARME_CONF_PATH "/home/edujoao/projeto-linux-embarcado/firmware/alarme.conf"

// =================================================================
// VARIÁVEIS GLOBAIS
// =================================================================

struct gpiod_chip *chip_botao = NULL;
struct gpiod_line_request *request_botao = NULL;
struct gpiod_chip *chip_buzzer = NULL;
struct gpiod_line_request *request_buzzer = NULL;

// Variáveis globais de controle
volatile int alarme_ativo = 0; 
volatile float limite_temp = 0.0;
volatile float temp_atual = 100.0; // Valor simulado para testes

void rotina_de_limpeza(int sinal) {
    printf("\n[Sinal recebido] Desligando hardware e liberando os pinos...\n");
    
    // Libera os recursos do Botão
    if (request_botao) gpiod_line_request_release(request_botao);
    if (chip_botao) gpiod_chip_close(chip_botao);

    // Libera os recursos do Buzzer
    if (request_buzzer) gpiod_line_request_release(request_buzzer);
    if (chip_buzzer) gpiod_chip_close(chip_buzzer);
    
    gmt130_encerrar(); // Inserção: Limpa e desliga o display SPI no encerramento
    ds18b20_liberar_hardware();

    exit(EXIT_SUCCESS);
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

    // 6. Força o Linux a ejetar a RAM e gravar fisicamente no disco via SPI
    fflush(arquivo);
    fsync(fileno(arquivo));
    fclose(arquivo);
}

void obter_ip_beaglebone(char *ip_buffer) {
    struct ifaddrs *ifaddr, *ifa;
    strcpy(ip_buffer, "Sem Rede"); 
    if (getifaddrs(&ifaddr) == -1) return;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "lo") != 0) {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
            strcpy(ip_buffer, inet_ntoa(pAddr->sin_addr));
            break;
        }
    }
    freeifaddrs(ifaddr);
}

void obter_espaco_sdcard(char *espaco_buffer) {
    struct statvfs stat;
    if (statvfs("/mnt/sdcard", &stat) != 0) {
        strcpy(espaco_buffer, "SD Desconectado");
        return;
    }
    double livres_mb = (double)(stat.f_bavail * stat.f_frsize) / (1024.0 * 1024.0);
    if (livres_mb >= 1024.0) snprintf(espaco_buffer, 32, "%.2f GB", livres_mb / 1024.0);
    else snprintf(espaco_buffer, 32, "%.1f MB", livres_mb);
}

/**
 * @brief Thread de monitoramento contínuo do sensor de temperatura.
 * * Realiza a inicialização do DS18B20 e executa um loop de leitura com intervalo
 * de 1 segundo. Leituras válidas são enviadas para armazenamento em CSV.
 * Falhas temporárias (como desconexão ou erro de CRC) são ignoradas silenciosamente
 * para manter a resiliência da execução contínua.
 * * @param arg Parâmetros genéricos da thread (não utilizado).
 * @return void* Sempre retorna NULL em caso de encerramento.
 */
void *thread_sensor_func(void *arg) {

    if (ds18b20_inicializar() != 0) {
        fprintf(stderr, "[Sensor] Falha fatal: Diretorio do barramento nao encontrado.\n");
        pthread_exit(NULL);
    }

    while (1) {
        temp_atual = ds18b20_ler_temperatura();
        printf("tSensor thread: temp_atual=%.2f\n", temp_atual);

        if (temp_atual == -1000.0f) {
            fprintf(stderr, "[Sensor] Falha temporaria de leitura ou CRC. Ignorando amostra.\n");
        } else {
            salvar_em_csv(temp_atual);
        }

        sleep(1); 
    }

    return NULL;
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
    while (1) {
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
        // Inserção: Alarme soa se exceder o limite OU se o sensor for desconectado
        printf("Buzzer thread: temp_atual=%.2f, limite_temp=%.2f\n", temp_atual, limite_temp);
        if (temp_atual >= limite_temp || temp_atual <= -999.0f) {
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

    // Inserção: Inicializa o display GMT130 no barramento SPI usando libgpiod
    if (gmt130_inicializar(SPI_DEVICE, DISPLAY_CHIP_PATH, LINHA_RES, LINHA_DC) != 0) {
        fprintf(stderr, "[Erro] Falha ao inicializar o display SPI.\n");
        return EXIT_FAILURE;
    }
    gmt130_limpar_tela(GMT130_PRETO); // Garante que a tela inicia apagada

    // 1. INICIA A THREAD DO INOTIFY (MONITORAMENTO DO ARQUIVO)
    pthread_t thread_inotify;
    pthread_create(&thread_inotify, NULL, thread_inotify_func, NULL);

    // 2. INICIA A THREAD DO BUZZER
    pthread_t thread_buzzer;
    pthread_create(&thread_buzzer, NULL, thread_buzzer_func, NULL);

    // 3. INICIA A THREAD DO SENSOR (Inserção: Adicionado para executar a leitura)
    pthread_t thread_sensor;
    pthread_create(&thread_sensor, NULL, thread_sensor_func, NULL);

    // 4. CONFIGURA O BOTÃO COMO ENTRADA
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

    printf("Sistema iniciado! Testando threads...\n");
    int estado_botao_anterior = 1;  
    
    // Variáveis necessárias para a lógica do display
    int tela_atual = 0;             
    float last_temp = -9999.0;      
    float last_limit = -9999.0;     
    int last_tela = -1;             
    char ip_buffer[32];             
    char sd_buffer[32];             
    char linha_buf[32];             

    for(;;) {
        // Leitura do botão com detecção de borda (debounce)
        int estado_botao = gpiod_line_request_get_value(request_botao, offset_botao);
        if (estado_botao == 0 && estado_botao_anterior == 1) {
            tela_atual = !tela_atual;
        }
        estado_botao_anterior = estado_botao;

        // Bloco de renderização gráfica SPI
        if (tela_atual == 0) {
            if (temp_atual != last_temp || limite_temp != last_limit || tela_atual != last_tela) {
                gmt130_limpar_tela(GMT130_PRETO);
                
                // Coordenadas em X e Y reais (ajustadas para visibilidade em matriz de pixels)
                gmt130_desenhar_texto(10, 20, "DATALOGGER", GMT130_VERDE, GMT130_PRETO);
                
                if (temp_atual <= -999.0f) {
                    gmt130_desenhar_texto(10, 60, "ALERTA CRITICO:", GMT130_VERMELHO, GMT130_PRETO);
                    gmt130_desenhar_texto(10, 80, ">> SENSOR OFF <<", GMT130_VERMELHO, GMT130_PRETO);
                } else {
                    snprintf(linha_buf, sizeof(linha_buf), "Temp:   %.1f C", temp_atual);
                    gmt130_desenhar_texto(10, 60, linha_buf, GMT130_BRANCO, GMT130_PRETO);
                    
                    snprintf(linha_buf, sizeof(linha_buf), "Alarme: %.1f C", limite_temp);
                    gmt130_desenhar_texto(10, 90, linha_buf, GMT130_BRANCO, GMT130_PRETO);
                }
                
                last_temp = temp_atual;
                last_limit = limite_temp;
                last_tela = tela_atual;
            }
        } else {
            if (tela_atual != last_tela) {
                obter_ip_beaglebone(ip_buffer);
                obter_espaco_sdcard(sd_buffer);
                
                gmt130_limpar_tela(GMT130_PRETO);
                gmt130_desenhar_texto(10, 20, "--- SISTEMA ---", GMT130_VERDE, GMT130_PRETO);
                
                snprintf(linha_buf, sizeof(linha_buf), "IP: %s", ip_buffer);
                gmt130_desenhar_texto(10, 60, linha_buf, GMT130_BRANCO, GMT130_PRETO);
                
                snprintf(linha_buf, sizeof(linha_buf), "SD: %s", sd_buffer);
                gmt130_desenhar_texto(10, 90, linha_buf, GMT130_BRANCO, GMT130_PRETO);
                
                last_tela = tela_atual;
                last_temp = -9999.0; // Força nova renderização ao voltar para a Tela 1
            }
        }

        usleep(100000); 
    }
    
    return EXIT_SUCCESS;
}