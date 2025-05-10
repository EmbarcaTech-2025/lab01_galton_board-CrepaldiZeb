#include "inc/oled_driver.h"
#include "pico/stdlib.h"
#include <string.h> // Pra usar o memset
#include <stdlib.h> // Pra usar malloc e free
#include <ctype.h>  

#include "inc/ssd1306.h"      // Funções da lib base do SSD1306
#include "inc/ssd1306_font.h" // Array 'font' com os desenhos dos caracteres

// Pinos e velocidade da comunicação I2C
#define I2C_PORT            i2c1
#define I2C_SDA_PIN         14
#define I2C_SCL_PIN         15
#define I2C_BAUDRATE        (ssd1306_i2c_clock * 1000) // Velocidade baseada na config do SSD1306

// Buffer interno para guardar o que vai ser desenhado e a área de renderização
static uint8_t *oled_buffer = NULL;
static struct render_area oled_area;

// Funçãozinha interna pra acender/apagar um pixel no buffer.
// Faz as checagens de limite pra gente não estourar nada.
static inline void _set_pixel_internal(uint8_t *buf, int x, int y, bool set) {
    if (!buf || x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return; // Fora da tela, nem tenta
    
    int page = y / 8; // Cada "página" tem 8 linhas de pixels
    int byte_idx = page * OLED_WIDTH + x; // A posição do byte no buffer
    uint8_t bit_pos = 1 << (y % 8);       // Qual bit dentro do byte representa o pixel

    if (byte_idx >= ssd1306_buffer_length) return; // Segurança extra

    if (set) {
        buf[byte_idx] |= bit_pos;  // Liga o bit
    } else {
        buf[byte_idx] &= ~bit_pos; // Desliga o bit
    }
}

bool oled_init(void) {
    // Configura a comunicação I2C com os pinos e velocidade definidos
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    sleep_ms(10); // Uma pequena pausa pra garantir que o I2C acordou bem

    ssd1306_init(); // Chama a inicialização da lib base do display

    // Define a área que a gente quer desenhar (a tela toda, no caso)
    oled_area.start_column = 0;
    oled_area.end_column   = OLED_WIDTH - 1;
    oled_area.start_page   = 0;
    oled_area.end_page     = ssd1306_n_pages - 1; // Número de páginas vem da lib base
    calculate_render_area_buffer_length(&oled_area); // Calcula o tamanho do buffer necessário

    // Aloca memória pro nosso buffer de desenho
    if (oled_buffer) free(oled_buffer); // Se já existia, libera antes
    oled_buffer = malloc(ssd1306_buffer_length); // Pega o tamanho da lib base
    if (!oled_buffer) return false; // Deu ruim na alocação de memória

    oled_clear_buffer(); // Limpa o buffer (tudo preto)
    oled_render();       // Manda o buffer limpo pra tela
    return true;
}

void oled_clear_buffer(void) {
    if (oled_buffer) {
        memset(oled_buffer, 0, ssd1306_buffer_length); // Preenche o buffer com zeros
    }
}

void oled_set_pixel(int x, int y, bool set) {
    _set_pixel_internal(oled_buffer, x, y, set); // Chama a função interna que faz o trabalho sujo
}

void oled_render(void) {
    if (oled_buffer) {
        render_on_display(oled_buffer, &oled_area); // Manda o buffer pra lib base desenhar
    }
}

void oled_deinit(void) {
     if (oled_buffer) {
        free(oled_buffer); // Libera a memória do buffer
        oled_buffer = NULL;
     }
     // i2c_deinit(I2C_PORT); // Desligar o I2C é opcional, depende se vai usar pra outra coisa
}

void oled_draw_vline(int x, int y_start, int height, bool set) {
    if (x < 0 || x >= OLED_WIDTH) return; // Se a linha começa fora, nem tenta
    int y_end = y_start + height;
    for (int y = y_start; y < y_end; ++y) {
        _set_pixel_internal(oled_buffer, x, y, set); // Acende/apaga cada pixel da linha
    }
}

void oled_draw_hline(int x_start, int y, int width, bool set) {
    if (y < 0 || y >= OLED_HEIGHT) return; // Se a linha começa fora, já era
    int x_end = x_start + width;
    for (int x = x_start; x < x_end; ++x) {
        _set_pixel_internal(oled_buffer, x, y, set); // Acende/apaga cada pixel da linha
    }
}

// Acha o "desenho" do caractere lá no array da fonte
static int _oled_get_font_char_offset(char character) {
    character = toupper(character); // A fonte só tem maiúsculas
    if (character >= 'A' && character <= 'Z') {
        return (character - 'A' + 1) * 8; // Cada caractere ocupa 8 bytes na fonte
    } else if (character >= '0' && character <= '9') {
        return (character - '0' + 27) * 8; // Números vêm depois das letras na fonte
    } else if (character == ':') {
        return 37 * 8; // Ajuste esse número se sua fonte for diferente!
    }
    return 0; // Se não achou, mostra um espaço (índice 0 da fonte)
}

// Desenha um caractere. O Y precisa ser alinhado com a página (múltiplo de 8).
static void oled_draw_char_page_aligned(int16_t x, int16_t y_page_aligned, char character) {
    // Checa se dá pra desenhar (dentro da tela e Y alinhado)
    if (!oled_buffer || x < 0 || x > (OLED_WIDTH - 8) || 
        y_page_aligned < 0 || y_page_aligned > (OLED_HEIGHT - 8) || (y_page_aligned % 8 != 0)) {
        return;
    }

    int char_font_offset = _oled_get_font_char_offset(character); // Pega o desenho na fonte
    int page_idx = y_page_aligned / 8; // Calcula a "linha" de páginas

    // Copia os 8 bytes (colunas de pixels) do caractere da fonte pro nosso buffer
    for (int i = 0; i < 8; ++i) { // Largura do caractere é 8 pixels/bytes
        int buffer_idx = page_idx * OLED_WIDTH + (x + i); // Onde vai no buffer
        if (buffer_idx < ssd1306_buffer_length) { // Mais uma checagem de segurança
             oled_buffer[buffer_idx] = font[char_font_offset + i]; // Copia o byte da fonte
        }
    }
}

// Escreve uma string inteira. O Y também precisa ser alinhado com a página (múltiplo de 8).
void oled_draw_string(int16_t x, int16_t y_page_aligned, const char *str) {
    if (!oled_buffer || (y_page_aligned % 8 != 0)) return; // Y desalinhado, não rola

    int16_t current_x = x;
    while (*str && current_x <= (OLED_WIDTH - 8)) { // Enquanto tiver letra e couber na tela
        oled_draw_char_page_aligned(current_x, y_page_aligned, *str++); // Desenha uma letra
        current_x += 8; // Avança pra próxima posição de caractere
    }
}