#include "ssd1306_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>

static int i2c_fd = -1;

// Buffer de vídeo do SSD1306 (128 colunas * 8 páginas = 1024 bytes)
static uint8_t framebuffer[1024];

// Fonte 5x8 básica simplificada
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
    [33] = {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (índice 65 - 32)
    [34] = {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    [35] = {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    [36] = {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    [37] = {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    [45] = {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    [51] = {0x3F, 0x40, 0x38, 0x40, 0x3F}, // S
    [52] = {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
};

// Função auxiliar para enviar um comando para o display
static void enviar_comando(uint8_t comando) {
    uint8_t buffer[2];
    buffer[0] = 0x00;
    buffer[1] = comando;
    if (write(i2c_fd, buffer, 2) != 2) {
        perror("[SSD1306] Erro ao enviar comando");
    }
}

int ssd1306_inicializar(const char *i2c_dev) {
    i2c_fd = open(i2c_dev, O_RDWR);
    if (i2c_fd < 0) {
        perror("[SSD1306] Erro ao abrir barramento I2C");
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0) {
        perror("[SSD1306] Erro ao configurar endereco I2C");
        close(i2c_fd);
        i2c_fd = -1;
        return -1;
    }

    enviar_comando(0xAE); 
    enviar_comando(0xD5); 
    enviar_comando(0x80); 
    enviar_comando(0xA8); 
    enviar_comando(0x3F); 
    enviar_comando(0xD3); 
    enviar_comando(0x00); 
    enviar_comando(0x40); 
    enviar_comando(0x8D); 
    enviar_comando(0x14); 
    enviar_comando(0x20); 
    enviar_comando(0x00); 
    enviar_comando(0xA1); 
    enviar_comando(0xC8); 
    enviar_comando(0xDA); 
    enviar_comando(0x12); 
    enviar_comando(0x81); 
    enviar_comando(0xCF); 
    enviar_comando(0xD9); 
    enviar_comando(0xF1);
    enviar_comando(0xDB); 
    enviar_comando(0x40);
    enviar_comando(0xA4); 
    enviar_comando(0xA6); 
    enviar_comando(0xAF); 

    ssd1306_limpar_buffer();
    ssd1306_atualizar_tela();

    printf("[SSD1306] Display I2C inicializado no endereco 0x%02X.\n", SSD1306_I2C_ADDR);
    return 0;
}

void ssd1306_encerrar(void) {
    if (i2c_fd >= 0) {
        ssd1306_limpar_buffer();
        ssd1306_atualizar_tela();
        enviar_comando(0xAE); // Display OFF antes de sair
        close(i2c_fd);
        i2c_fd = -1;
        printf("[SSD1306] Display desligado e arquivos fechados.\n");
    }
}

void ssd1306_limpar_buffer(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

void ssd1306_desenhar_texto(int x, int pagina_y, const char *texto) {
    if (x < 0 || x > 127 || pagina_y < 0 || pagina_y > 7) return;

    int cursor_x = x;
    while (*texto) {
        char c = *texto;
        if (c >= 32 && c <= 127) {
            int char_idx = c - 32;
            for (int i = 0; i < 5; i++) {
                if (cursor_x + i < 128) {
                    // Mapeia na estrutura do framebuffer [página * 128 + coluna]
                    framebuffer[pagina_y * 128 + cursor_x + i] = fonte_5x8[char_idx][i];
                }
            }
        }
        cursor_x += 6; // Largura da fonte (5px) + 1 pixel de espaçamento
        texto++;
    }
}

void ssd1306_atualizar_tela(void) {
    if (i2c_fd < 0) return;

    enviar_comando(0x21);
    enviar_comando(0x00);
    enviar_comando(0x7F);
    
    enviar_comando(0x22);
    enviar_comando(0x00);
    enviar_comando(0x07);

    // O barramento I2C requer o byte de controle (0x40 para dados) 
    // antes de cada pacote de envio. Como o buffer de 1024 bytes é grande
    // para alguns drivers I2C, enviamos de bloco em bloco (por página).
    uint8_t i2c_buffer[129];
    i2c_buffer[0] = 0x40; // Co = 0, D/C = 1 (Dados)

    for (int pagina = 0; pagina < 8; pagina++) {
        memcpy(&i2c_buffer[1], &framebuffer[pagina * 128], 128);
        if (write(i2c_fd, i2c_buffer, 129) != 129) {
            perror("[SSD1306] Erro ao enviar framebuffer");
            break;
        }
    }
}