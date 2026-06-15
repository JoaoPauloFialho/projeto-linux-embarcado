#include "gmt130_spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>

static int spi_fd = -1;
static struct gpiod_chip *chip_display = NULL;
static struct gpiod_line_request *req_dc = NULL;
static struct gpiod_line_request *req_res = NULL;
static unsigned int linha_dc_global;

// Fonte básica 5x8 simplificada para caracteres ASCII imprimíveis
static const uint8_t fonte_5x8[96][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Espaço (32)
    {0x00, 0x00, 0x4F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    // Nota: O mapa ASCII foi encurtado para fins de brevidade e foca nos numéricos e maiúsculas comuns.
    // Para um projeto real, você pode substituir este bloco por uma matriz completa de fonte 5x8.
    [33] = {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (índice 65 - 32)
    [34] = {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    [35] = {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    [36] = {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    [37] = {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    [45] = {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    [51] = {0x3F, 0x40, 0x38, 0x40, 0x3F}, // S
    [52] = {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    // Adicione os demais conforme a necessidade do seu projeto
};

static void enviar_spi(uint8_t dc, const uint8_t *dados, size_t tamanho) {
    gpiod_line_request_set_value(req_dc, linha_dc_global, dc);
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)dados,
        .rx_buf = (unsigned long)NULL,
        .len = tamanho,
        .speed_hz = 10000000, // 10 MHz
        .bits_per_word = 8,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static void enviar_comando(uint8_t cmd) {
    enviar_spi(0, &cmd, 1); // DC = 0 para comando
}

static void enviar_dado(uint8_t dado) {
    enviar_spi(1, &dado, 1); // DC = 1 para dado
}

static void definir_janela(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    enviar_comando(0x2A); // CASET
    enviar_dado(x0 >> 8); enviar_dado(x0 & 0xFF);
    enviar_dado(x1 >> 8); enviar_dado(x1 & 0xFF);
    
    enviar_comando(0x2B); // RASET
    enviar_dado(y0 >> 8); enviar_dado(y0 & 0xFF);
    enviar_dado(y1 >> 8); enviar_dado(y1 & 0xFF);
    
    enviar_comando(0x2C); // RAMWR
}

int gmt130_inicializar(const char *spi_dev, const char *gpio_chip, int pino_res, int pino_dc) {
    linha_dc_global = pino_dc;

    // 1. Configura SPI
    spi_fd = open(spi_dev, O_RDWR);
    if (spi_fd < 0) {
        perror("[GMT130] Falha ao abrir SPI");
        return -1;
    }
    uint8_t mode = SPI_MODE_0;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    // 2. Configura GPIOs (RES e DC)
    chip_display = gpiod_chip_open(gpio_chip);
    if (!chip_display) return -1;

    struct gpiod_line_settings *cfg_out = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(cfg_out, GPIOD_LINE_DIRECTION_OUTPUT);

    struct gpiod_line_config *cfg_line_dc = gpiod_line_config_new();
    unsigned int off_dc = pino_dc;
    gpiod_line_config_add_line_settings(cfg_line_dc, &off_dc, 1, cfg_out);

    struct gpiod_line_config *cfg_line_res = gpiod_line_config_new();
    unsigned int off_res = pino_res;
    gpiod_line_config_add_line_settings(cfg_line_res, &off_res, 1, cfg_out);

    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "gmt130_ctrl");

    req_dc = gpiod_chip_request_lines(chip_display, req_cfg, cfg_line_dc);
    req_res = gpiod_chip_request_lines(chip_display, req_cfg, cfg_line_res);

    // Limpeza de recursos
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(cfg_line_dc);
    gpiod_line_config_free(cfg_line_res);
    gpiod_line_settings_free(cfg_out);

    // 3. Reset do Hardware
    gpiod_line_request_set_value(req_res, off_res, 1);
    usleep(10000);
    gpiod_line_request_set_value(req_res, off_res, 0);
    usleep(10000);
    gpiod_line_request_set_value(req_res, off_res, 1);
    usleep(120000);

    // 4. Inicialização do Controlador (Comandos básicos ST7789)
    enviar_comando(0x11); // Sleep Out
    usleep(120000);
    enviar_comando(0x3A); // Color Mode
    enviar_dado(0x05);    // 16-bit RGB565
    enviar_comando(0x29); // Display ON
    usleep(10000);

    return 0;
}

void gmt130_encerrar(void) {
    if (spi_fd >= 0) close(spi_fd);
    if (req_dc) gpiod_line_request_release(req_dc);
    if (req_res) gpiod_line_request_release(req_res);
    if (chip_display) gpiod_chip_close(chip_display);
}

void gmt130_limpar_tela(uint16_t cor) {
    definir_janela(0, 0, 239, 239); // Resolução típica
    uint8_t cor_bytes[2] = { cor >> 8, cor & 0xFF };
    uint8_t buffer[480]; // Buffer para uma linha
    for(int i=0; i<240; i++) {
        buffer[i*2] = cor_bytes[0];
        buffer[i*2+1] = cor_bytes[1];
    }
    // Envia linha por linha preenchendo a tela
    for(int i=0; i<240; i++) {
        enviar_spi(1, buffer, sizeof(buffer));
    }
}

void gmt130_desenhar_texto(int x, int y, const char *texto, uint16_t cor_texto, uint16_t cor_fundo) {
    int cursor_x = x;
    while (*texto) {
        char c = *texto;
        if (c >= 32 && c <= 127) {
            int char_idx = c - 32;
            for (int i = 0; i < 5; i++) { // Largura da fonte
                uint8_t linha = fonte_5x8[char_idx][i];
                for (int j = 0; j < 8; j++) { // Altura da fonte
                    if (linha & (1 << j)) {
                        // Pixel ativo
                        definir_janela(cursor_x + i, y + j, cursor_x + i, y + j);
                        uint8_t cor_arr[2] = { cor_texto >> 8, cor_texto & 0xFF };
                        enviar_spi(1, cor_arr, 2);
                    } else {
                        // Fundo
                        definir_janela(cursor_x + i, y + j, cursor_x + i, y + j);
                        uint8_t cor_arr[2] = { cor_fundo >> 8, cor_fundo & 0xFF };
                        enviar_spi(1, cor_arr, 2);
                    }
                }
            }
        }
        cursor_x += 6; // Avança para o próximo caractere (5px largura + 1px espaço)
        texto++;
    }
}