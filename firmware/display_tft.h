#ifndef DISPLAY_TFT_H
#define DISPLAY_TFT_H

// Inicializa o mapeamento de memória (mmap) com o Framebuffer do Linux
void tft_inicializar(void);

// Limpa toda a tela (preenche com a cor preta) para evitar sobreposição de texto
void tft_limpar_tela(void);

// Desenha a Tela 1: Mostra a Temperatura Atual e o Limite do Alarme
void tft_exibir_tela_principal(float temperatura_atual, float limite_alarme);

// Desenha a Tela 2: Mostra o IP da BeagleBone e o Espaço Livre no SD Card
void tft_exibir_tela_rede(const char *ip_rede, const char *espaco_sd);

#endif // DISPLAY_TFT_H