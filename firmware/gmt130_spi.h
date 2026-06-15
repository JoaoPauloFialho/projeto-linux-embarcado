#ifndef GMT130_SPI_H
#define GMT130_SPI_H

#include <stdint.h>

// Cores básicas no formato RGB565
#define GMT130_PRETO    0x0000
#define GMT130_BRANCO   0xFFFF
#define GMT130_VERMELHO 0xF800
#define GMT130_VERDE    0x07E0
#define GMT130_AZUL     0x001F

/**
 * Inicializa o barramento SPI e os GPIOs (DC e RES) do display.
 * @param spi_dev Caminho para o device SPI (ex: "/dev/spidev0.0")
 * @param gpio_chip Caminho para o chip GPIO (ex: "/dev/gpiochip2")
 * @param pino_res Linha GPIO correspondente ao pino RES
 * @param pino_dc Linha GPIO correspondente ao pino DC
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int gmt130_inicializar(const char *spi_dev, const char *gpio_chip, int pino_res, int pino_dc);

/**
 * Libera os recursos de hardware do display.
 */
void gmt130_encerrar(void);

/**
 * Preenche a tela inteira com uma cor específica.
 */
void gmt130_limpar_tela(uint16_t cor);

/**
 * Desenha um texto na tela (coordenadas em pixels).
 */
void gmt130_desenhar_texto(int x, int y, const char *texto, uint16_t cor_texto, uint16_t cor_fundo);

#endif // GMT130_SPI_H