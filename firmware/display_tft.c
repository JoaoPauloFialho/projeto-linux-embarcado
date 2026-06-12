#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "font8x8.h"

// Cores Básicas (Formato RGB565)
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_RED    0xF800
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F

static int fbfd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static long int screensize = 0;
static char *fbp = 0;

void tft_inicializar(void) {
    // Tenta abrir o fb1 (padrão de displays secundários)
    fbfd = open("/dev/fb1", O_RDWR);
    if (fbfd == -1) {
        printf("Tentando /dev/fb0...\n");
        fbfd = open("/dev/fb0", O_RDWR);
        if (fbfd == -1) {
            perror("Erro: Nao foi possivel abrir o framebuffer");
            exit(1);
        }
    }

    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
    screensize = vinfo.xres_virtual * vinfo.yres_virtual * vinfo.bits_per_pixel / 8;
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    
    if ((intptr_t)fbp == -1) {
        perror("Erro no mmap");
        exit(1);
    }
}

void tft_limpar_tela(void) {
    if (fbp) memset(fbp, 0x00, screensize);
}

void desenhar_pixel(int x, int y, unsigned short int color) {
    if (x < 0 || x >= vinfo.xres || y < 0 || y >= vinfo.yres) return;
    long int location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) + 
                        (y + vinfo.yoffset) * finfo.line_length;
    *((unsigned short int*)(fbp + location)) = color;
}

void desenhar_retangulo(int x0, int y0, int largura, int altura, unsigned short int color) {
    for (int y = y0; y < y0 + altura; y++) {
        for (int x = x0; x < x0 + largura; x++) {
            desenhar_pixel(x, y, color);
        }
    }
}

// Lógica de leitura da matriz de fonte 8x8 e escrita na tela
// Adicionado fator de escala para o texto não ficar muito pequeno
void desenhar_caractere(int x, int y, char c, unsigned short cor_texto, int escala) {
    if (c < 32 || c > 127) c = '?';
    const unsigned char *glyph = font8x8[c - 32];

    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (glyph[cy] & (1 << cx)) {
                // Desenha blocos maiores baseado na escala
                desenhar_retangulo(x + (cx * escala), y + (cy * escala), escala, escala, cor_texto);
            }
        }
    }
}

void desenhar_texto(int x, int y, const char *texto, unsigned short cor_texto, int escala) {
    while (*texto) {
        desenhar_caractere(x, y, *texto, cor_texto, escala);
        x += (8 * escala); // Avança o cursor
        texto++;
    }
}

void tft_exibir_tela_principal(float temperatura_atual, float limite_alarme) {
    char buffer[64];
    tft_limpar_tela();
    
    desenhar_retangulo(0, 0, vinfo.xres, 40, COLOR_BLUE);
    desenhar_texto(10, 10, "TEMPERATURA", COLOR_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "Atual: %.1f C", temperatura_atual);
    unsigned short cor = (temperatura_atual >= limite_alarme) ? COLOR_RED : COLOR_GREEN;
    desenhar_texto(10, 80, buffer, cor, 2);

    snprintf(buffer, sizeof(buffer), "Alarme: %.1f C", limite_alarme);
    desenhar_texto(10, 140, buffer, COLOR_WHITE, 2);
}

void tft_exibir_tela_rede(const char *ip_rede, const char *espaco_sd) {
    char buffer[64];
    tft_limpar_tela();
    
    desenhar_retangulo(0, 0, vinfo.xres, 40, COLOR_GREEN);
    desenhar_texto(10, 10, "SISTEMA", COLOR_BLACK, 2);

    snprintf(buffer, sizeof(buffer), "IP: %s", ip_rede);
    desenhar_texto(10, 80, buffer, COLOR_WHITE, 2);

    snprintf(buffer, sizeof(buffer), "SD: %s", espaco_sd);
    desenhar_texto(10, 140, buffer, COLOR_WHITE, 2);
}