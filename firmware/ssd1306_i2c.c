#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "ssd1306_i2c.h"
#include "font8x8.h"

#define SSD1306_I2C_ADDR 0x3C

static int i2c_fd = -1;
static uint8_t fb[1024]; // Framebuffer de 128x64 pixels (1024 bytes)

static void enviar_comando(uint8_t cmd) {
    uint8_t buffer[2] = {0x00, cmd};
    write(i2c_fd, buffer, 2);
}

int ssd1306_inicializar(const char *i2c_device) {
    i2c_fd = open(i2c_device, O_RDWR);
    if (i2c_fd < 0) {
        perror("Falha ao abrir o barramento I2C");
        return -1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0) {
        perror("Falha ao se conectar ao endereco do Display");
        return -1;
    }

    // Sequência padrão de inicialização do controlador SSD1306
    enviar_comando(0xAE); // Display OFF
    enviar_comando(0x20); // Set Memory Addressing Mode
    enviar_comando(0x00); // Horizontal Addressing Mode
    enviar_comando(0x8D); // Charge Pump
    enviar_comando(0x14); // Enable Charge Pump
    enviar_comando(0xAF); // Display ON

    ssd1306_limpar_buffer();
    ssd1306_atualizar_tela();
    return 0;
}

void ssd1306_limpar_buffer(void) {
    memset(fb, 0, sizeof(fb));
}

void ssd1306_desenhar_texto(int x, int linha_y, const char *texto) {
    if (linha_y < 0 || linha_y > 7) return;
    
    while (*texto) {
        if (x > 120) break; // Limite horizontal
        unsigned char c = *texto;
        if (c < 32 || c > 127) c = '?';
        
        const unsigned char *glyph = font8x8[c - 32];
        for (int i = 0; i < 8; i++) {
            fb[(linha_y * 128) + x + i] = glyph[i];
        }
        x += 8; // Avança o cursor para a próxima letra
        texto++;
    }
}

void ssd1306_atualizar_tela(void) {
    enviar_comando(0x21); // Column Address
    enviar_comando(0);    // Start
    enviar_comando(127);  // End
    enviar_comando(0x22); // Page Address
    enviar_comando(0);    // Start
    enviar_comando(7);    // End

    // Envia os dados com o byte de controle para Data (0x40)
    uint8_t buffer[1025];
    buffer[0] = 0x40;
    memcpy(&buffer[1], fb, 1024);
    write(i2c_fd, buffer, 1025);
}

void ssd1306_encerrar(void) {
    ssd1306_limpar_buffer();
    ssd1306_atualizar_tela();
    enviar_comando(0xAE); // Display OFF
    if (i2c_fd >= 0) close(i2c_fd);
}