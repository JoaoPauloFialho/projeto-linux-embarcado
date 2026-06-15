#ifndef SSD1306_I2C_H
#define SSD1306_I2C_H

#include <stdint.h>

#define SSD1306_I2C_ADDR 0x3C // Endereço I2C padrão (pode ser 0x3D dependendo do módulo)

/**
 * Inicializa o barramento I2C e o controlador do display.
 * @param i2c_dev Caminho para o device I2C (ex: "/dev/i2c-2")
 * @return 0 em caso de sucesso, -1 em caso de erro.
 */
int ssd1306_inicializar(const char *i2c_dev);

/**
 * Libera os recursos do hardware e fecha o arquivo I2C.
 */
void ssd1306_encerrar(void);

/**
 * Limpa o buffer interno (memória RAM local), deixando a tela "preta".
 * IMPORTANTE: Essa função não atualiza a tela física, apenas o buffer.
 */
void ssd1306_limpar_buffer(void);

/**
 * Desenha um texto no buffer local.
 * Para o SSD1306 (128x64), a tela é dividida em 8 "páginas" horizontais.
 * @param x Posição horizontal (0 a 127)
 * @param pagina_y Posição vertical em páginas (0 a 7, onde cada página tem 8 pixels de altura)
 * @param texto String a ser impressa
 */
void ssd1306_desenhar_texto(int x, int pagina_y, const char *texto);

/**
 * Envia todo o buffer local para a tela via I2C, atualizando a imagem real.
 */
void ssd1306_atualizar_tela(void);

#endif // SSD1306_I2C_H