#ifndef SSD1306_I2C_H
#define SSD1306_I2C_H

#include <stdint.h>

// Inicializa o barramento I2C (ex: "/dev/i2c-2") e configura o display OLED
int ssd1306_inicializar(const char *i2c_device);

// Limpa o buffer de memória interna (não atualiza a tela ainda)
void ssd1306_limpar_buffer(void);

// Escreve um texto no buffer nas coordenadas X (0-127) e Y (0-7, onde cada passo é uma linha de 8 pixels)
void ssd1306_desenhar_texto(int x, int linha_y, const char *texto);

// Envia o buffer completo para o display I2C de uma só vez (Anti-Flicker)
void ssd1306_atualizar_tela(void);

// Desliga o display e fecha o arquivo I2C
void ssd1306_encerrar(void);

#endif