#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"
#include "ssd1306_i2c.h" // Pra pegar as definições de largura/altura do SSD1306

// Largura e altura do nosso display OLED, pegando da lib base
#define OLED_WIDTH  ssd1306_width
#define OLED_HEIGHT ssd1306_height

// --- Funções da Interface Pública do Driver ---

// Prepara o display para uso
bool oled_init(void);

// Limpa todo o conteúdo do buffer (deixa tudo preto)
void oled_clear_buffer(void);

// Acende ou apaga um pixel específico no buffer
void oled_set_pixel(int x, int y, bool set);

// Envia o conteúdo do buffer para o display (desenha na tela)
void oled_render(void);

// Libera recursos usados pelo driver (se necessário)
void oled_deinit(void);

// Desenha uma linha vertical no buffer
void oled_draw_vline(int x, int y, int height, bool set);

// Desenha uma linha horizontal no buffer
void oled_draw_hline(int x, int y, int width, bool set);

// Escreve uma string no buffer. O Y precisa ser alinhado com a página
void oled_draw_string(int16_t x, int16_t y_page_aligned, const char *str);

#endif // OLED_DRIVER_H