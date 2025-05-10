#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h" 
#include "hardware/gpio.h"
#include "inc/oled_driver.h"   // Nosso driver OLED

// --- Configurações da Simulação ---
#define FRAME_INTERVAL_MS   20   // Intervalo entre "frames", afeta a velocidade
#define MAX_BALLS           50   // Quantas bolinhas podem estar na tela ao mesmo tempo
#define BUTTON_DROP_BALL_PIN 6   // Pino do botão de soltar bolinha
#define BUTTON_SWITCH_SCREEN_PIN 5 // Pino do botão de trocar tela
#define DEBOUNCE_DELAY_US   (200 * 1000) // 200ms para evitar repetição de clique no botão

// --- Configurações da Tábua de Galton ---
#define START_X             0                 // Onde as bolinhas começam no eixo X
#define START_Y             (OLED_HEIGHT / 2) // Onde as bolinhas começam no eixo Y (meio da tela)
#define FINAL_X             (OLED_WIDTH - 1)  // Onde as bolinhas param no eixo X (final da tela)
#define VERTICAL_DEFLECTION 3                 // O quanto a bolinha desvia pra cima ou pra baixo
// Chance (0-100) da bolinha ir pra BAIXO (Y maior). 50 = 50% pra cima, 50% pra baixo.
#define PROBABILITY_POSITIVE_Y_DEFLECTION_PERCENT 50 

// --- Configurações para os "Pinos" Virtuais ---
#define PIN_START_X          10  // Posição X do primeiro "pino"
#define NUM_PIN_LEVELS       11  // Quantos níveis de "pinos" teremos
#define PIN_X_INCREMENT      2 // Distância horizontal entre os "pinos" (quanto somar no X para o próximo pino)

// Pra saber qual tela estamos mostrando
typedef enum {
    SCREEN_GAME,
    SCREEN_SCORE
} ScreenState;

// Guarda o estado de cada bolinha
typedef struct {
    int   x_pos;
    float y_pos;             // Usar float pra y_pos ajuda a ter um movimento mais suave antes de arredondar pro pixel
    int   current_pin_level; // Qual o próximo nível de "pino" que a bolinha está mirando (0 para o primeiro)
    bool  active;            // Se a bolinha está se movendo ou já parou
} BallState;

// --- Variáveis Globais da Simulação ---
static BallState balls[MAX_BALLS];                 // Array com todas as bolinhas
static int ball_add_index = 0;                   // Pra saber onde adicionar a próxima bolinha
static uint64_t last_drop_button_press_time = 0; // Pra controle do debounce do botão de soltar
static uint64_t last_switch_button_press_time = 0;// Pra controle do debounce do botão de trocar tela
static uint8_t stack_depth_at_y[OLED_HEIGHT];    // Conta quantas bolinhas pararam em cada linha Y final
static ScreenState current_screen = SCREEN_GAME;   // Começa na tela do jogo
static uint32_t total_balls_dropped_count = 0;   // Contador total de bolinhas soltas

// --- Funções Auxiliares (Protótipos) ---
static void init_buttons(void);
static bool check_button_press(uint pin, uint64_t *last_press_time); // Função genérica pra botão
static void add_new_ball(void);
static void update_ball_state(BallState *ball);
static void update_all_balls(void);
static void render_galton_board(void);
static void render_score_screen(void);

// --- Função Principal (main) ---
int main(void) {
    stdio_init_all();    // Inicializa a comunicação serial (USB) pra debug, se precisar
    init_buttons();      // Configura os pinos dos botões
    srand(time_us_32()); // Semente pro gerador de números aleatórios, pra não ser sempre igual

    if (!oled_init()) { // Tenta ligar e configurar o display OLED
        // Se deu ruim aqui, melhor nem continuar...
        while(1) tight_loop_contents(); 
    }

    memset(stack_depth_at_y, 0, sizeof(stack_depth_at_y)); // Zera o contador de bolinhas nas colunas finais
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false; // Começa com todas as bolinhas "desligadas"
    }

    // Loop principal do programa
    while (1) {
        // Checa se o botão de trocar tela foi apertado
        if (check_button_press(BUTTON_SWITCH_SCREEN_PIN, &last_switch_button_press_time)) {
            current_screen = (current_screen == SCREEN_GAME) ? SCREEN_SCORE : SCREEN_GAME; // Alterna a tela
        }

        if (current_screen == SCREEN_GAME) { // Se estiver na tela do jogo...
            if (check_button_press(BUTTON_DROP_BALL_PIN, &last_drop_button_press_time)) {
                add_new_ball(); // Solta uma nova bolinha
            }
            update_all_balls();     // Move todas as bolinhas ativas
            render_galton_board();  // Desenha a tábua de Galton e as bolinhas
        } else { // Senão, está na tela de pontuação
            render_score_screen(); // Mostra a pontuação
        }
        sleep_ms(FRAME_INTERVAL_MS); // Pausa um pouquinho pra controlar a velocidade da animação
    }
    // oled_deinit(); // Se o programa pudesse sair do loop, seria bom desligar o OLED aqui
    return 0; // Teoricamente, nunca chega aqui
}

// --- Implementações das Funções ---

static void init_buttons(void) {
    // Botão de soltar bolinha
    gpio_init(BUTTON_DROP_BALL_PIN);
    gpio_set_dir(BUTTON_DROP_BALL_PIN, GPIO_IN); // Configura como entrada
    gpio_pull_up(BUTTON_DROP_BALL_PIN);          // Habilita resistor de pull-up interno

    // Botão de trocar tela
    gpio_init(BUTTON_SWITCH_SCREEN_PIN);
    gpio_set_dir(BUTTON_SWITCH_SCREEN_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_SWITCH_SCREEN_PIN);
}

// Função genérica para checar um botão com debounce
static bool check_button_press(uint pin, uint64_t *last_press_time) {
    uint64_t now = time_us_64(); // Pega o tempo atual em microssegundos
    // Só processa o clique se já passou o tempo de debounce desde o último clique
    if (now - *last_press_time > DEBOUNCE_DELAY_US) {
        if (!gpio_get(pin)) { // Se o pino está em nível baixo (botão pressionado)
            *last_press_time = now; // Atualiza o tempo do último clique
            return true;            // Botão foi pressionado!
        }
    }
    return false; // Nada de clique novo
}

static void add_new_ball(void) {
    int index_to_use = -1;
    // Procura um "slot" de bolinha que não esteja em uso
    for(int i = 0; i < MAX_BALLS; ++i) {
        int check_idx = (ball_add_index + i) % MAX_BALLS; // Faz a busca de forma circular
        if (!balls[check_idx].active) {
            index_to_use = check_idx;
            break;
        }
    }
    // Se todos os slots estiverem ocupados, substitui a bolinha mais "antiga" (estratégia simples)
    if (index_to_use == -1) index_to_use = ball_add_index; 

    // Configura a nova bolinha
    balls[index_to_use] = (BallState) {
        .x_pos = START_X,
        .y_pos = (float)START_Y,    // Começa no meio da tela em Y
        .current_pin_level = 0,     // Ainda não passou por nenhum "pino"
        .active = true              // Marca como ativa
    };
    ball_add_index = (index_to_use + 1) % MAX_BALLS; // Prepara o índice pro próximo add
}

static void update_ball_state(BallState *ball) {
    if (!ball->active) return; // Se a bolinha não está ativa, não faz nada

    ball->x_pos += 1; // Bolinha avança uma coluna pra direita

    // Checa se a bolinha ainda tem "pinos" pela frente
    if (ball->current_pin_level < NUM_PIN_LEVELS) {
        // Calcula a posição X do "pino" atual que a bolinha está se aproximando
        int current_pin_x_target = PIN_START_X + (ball->current_pin_level * PIN_X_INCREMENT);

        if (ball->x_pos >= current_pin_x_target) { // Chegou no "pino" ou passou um pouquinho
            // Sorteia se vai pra cima ou pra baixo
            int deflection = (rand() % 100 < PROBABILITY_POSITIVE_Y_DEFLECTION_PERCENT) ? 1 : -1;
            ball->y_pos += deflection * VERTICAL_DEFLECTION; // Aplica o desvio

            // Garante que a bolinha não saia da tela verticalmente
            if (ball->y_pos < 0) ball->y_pos = 0;
            if (ball->y_pos >= OLED_HEIGHT) ball->y_pos = OLED_HEIGHT - 1;

            ball->current_pin_level++; // Prepara para o próximo nível de "pino"
        }
    }

    // Se a bolinha chegou no final da tela...
    if (ball->x_pos >= FINAL_X) {
        ball->x_pos = FINAL_X;  // Trava na posição final
        ball->active = false;   // Marca como inativa
        
        int final_y = (int)(ball->y_pos + 0.5f); // Arredonda a posição Y final
        // Mais uma checagem de segurança pra Y
        if (final_y < 0) final_y = 0;
        if (final_y >= OLED_HEIGHT) final_y = OLED_HEIGHT - 1;
        
        // Incrementa a "pilha" de bolinhas naquela linha Y, se não estourar a largura visual
        if (stack_depth_at_y[final_y] < OLED_WIDTH) { 
            stack_depth_at_y[final_y]++;
        }
        total_balls_dropped_count++; // Conta mais uma bolinha que caiu
    }
}

static void update_all_balls(void) {
    for (int i = 0; i < MAX_BALLS; ++i) {
         update_ball_state(&balls[i]); // Atualiza cada bolinha
    }
}

static void render_galton_board(void) {
    oled_clear_buffer(); // Limpa o que tinha antes

    // Desenha as pilhas de bolinhas que já caíram
    for (int y = 0; y < OLED_HEIGHT; ++y) { // Pra cada linha Y da tela
        for (int h = 0; h < stack_depth_at_y[y]; ++h) { // Quantas bolinhas empilhadas ali
            int pixel_x = (OLED_WIDTH - 1) - h; // Empilha da direita pra esquerda
            if (pixel_x >= 0) { // Só desenha se estiver dentro da tela
                oled_set_pixel(pixel_x, y, true);
            } else {
                break; // Já saiu da tela, não precisa continuar
            }
        }
    }

    // Desenha as bolinhas que ainda estão caindo
    for (int i = 0; i < MAX_BALLS; ++i) {
        if (balls[i].active) {
            int y_render = (int)(balls[i].y_pos + 0.5f); // Arredonda Y pra desenhar
            // Só desenha se a bolinha estiver visível na tela
            if (balls[i].x_pos >= 0 && balls[i].x_pos < OLED_WIDTH && 
                y_render >= 0 && y_render < OLED_HEIGHT) {
                 oled_set_pixel(balls[i].x_pos, y_render, true);
            }
        }
    }
    oled_render(); // Manda tudo pro display!
}

static void render_score_screen(void) {
    oled_clear_buffer(); // Limpa a tela
    char score_text[20]; // Buffer pra string da pontuação
    
    // Escreve "TOTAL:" (Y=16 é a segunda linha de "páginas" de 8 pixels)
    oled_draw_string(16, 16, "TOTAL "); 
    
    // Converte o número total de bolinhas para string
    snprintf(score_text, sizeof(score_text), "%lu", total_balls_dropped_count); 
    
    // Calcula a posição X pra centralizar o número na tela
    int text_width = strlen(score_text) * 8; // Cada caractere da fonte tem 8 pixels de largura
    int x_pos = (OLED_WIDTH - text_width) / 2;
    if (x_pos < 0) x_pos = 0; // Garante que não comece fora da tela

    // Escreve o número (Y=32 é a quarta linha de "páginas")
    oled_draw_string(x_pos, 32,score_text); 
    
    oled_render(); // Manda pra tela
}