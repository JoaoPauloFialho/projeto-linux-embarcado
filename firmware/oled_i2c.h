#ifndef OLED_I2C_H
#define OLED_I2C_H

#include <stdint.h>

// Inicializa a comunicação I2C e o display OLED SSD1306
// Retorna 0 em caso de sucesso, -1 em caso de erro
int init_oled(const char *i2c_bus, int i2c_addr);

// Define onde o próximo texto será impresso (coluna 0 a 127, página 0 a 7)
void ssd1306_set_cursor(uint8_t col, uint8_t page);

// Limpa toda a tela (preenche com zeros)
void ssd1306_clear(void);

// Imprime uma string no display a partir da posição atual do cursor
void oled_print(const char *str);

// Fecha a conexão do barramento I2C
void close_oled(void);

#endif // OLED_I2C_H