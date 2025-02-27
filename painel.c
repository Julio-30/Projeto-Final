// Bibliotecas utilizadas
#include "hardware/pio.h"  
#include "hardware/i2c.h" 
#include "pico/stdlib.h" 
#include "ssd1306.h"       
#include <stdlib.h>   
#include <stdio.h> 
#include <math.h> 
#include "font.h" 
#include "ws2818b.pio.h"// Biblioteca para controlar matriz de LEDs WS2818B
#include "hardware/timer.h"

// Definindo pinos para comunicação I2C
#define I2C_PORT i2c1   
#define I2C_SDA_PIN 14     
#define I2C_SCL_PIN 15   
#define DISPLAY_ADDRESS 0x3C // Endereço I2C do display SSD1306

// Definindo os pinos dos LEDs e botões
#define LED_VERDE 11
#define LED_AZUL 12
#define LED_VERMELHO 13
#define BOTAO_VERDE 5
#define BOTAO_ALTERNAR 6

#define MATRIX_PIN 7 // Pino de controle da matriz de LEDs
#define NUM_LEDS 25  // Número total de LEDs na matriz

static uint32_t last_time = 0; // Declaração correta da variável
uint16_t estado_led = 0;
bool color = true;

void init_leds() {
    // Configura os pinos dos LEDs como saída
    gpio_init(LED_VERDE);
    gpio_init(LED_AZUL);
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
}

ssd1306_t display;
// Inicialização e configurar do I2C e do display OLED SSD1306 
void init_display() {
    i2c_init(I2C_PORT, 400 * 1000); // Comunicação I2C com velocidade de 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);  // Configura o pino SDA para função I2C
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);  // Configura o pino SCL para função I2C
    gpio_pull_up(I2C_SDA_PIN); 
    gpio_pull_up(I2C_SCL_PIN); 

    ssd1306_init(&display, WIDTH, HEIGHT, false, DISPLAY_ADDRESS, I2C_PORT); // Inicializa o display SSD1306 com dimensões e endereço I2C
    ssd1306_config(&display);     // Configura o display com parâmetros adicionais
    ssd1306_send_data(&display);  // Envia dados iniciais ao display

    ssd1306_fill(&display, false); // Limpa o display com pixels apagados
    ssd1306_send_data(&display);   // Atualiza o display para refletir a limpeza
}

void alternar_leds(uint16_t *estado_led) { 
    // Verifica se o botão de alternar foi pressionado (estado baixo, pois está com pull-up)
    switch (*estado_led) {
       case 0:
          gpio_put(LED_AZUL, 1); // Acende o LED azul
          gpio_put(LED_VERMELHO, 0); // Apaga o LED vermelho
          //ssd1306_draw_string(&display, "Gas 50", 10, 50);
          //ssd1306_send_data(&display); // Envia os dados para atualizar o display
          //seta1();
          break;
       case 1:
          gpio_put(LED_AZUL, 0); // Apaga o LED azul
          gpio_put(LED_VERMELHO, 1); // Acende o LED vermelho
          //ssd1306_draw_string(&display, "Gas 25", 10, 50);
          //ssd1306_send_data(&display); // Envia os dados para atualizar o display
          //seta2();
          break;
       case 2:
          gpio_put(LED_AZUL, 0); // Apaga o LED azul
          gpio_put(LED_VERMELHO, 0); // Apaga o LED vermelho
          //ssd1306_fill(&display, !color);
          //ssd1306_rect(&display, 3, 3, 122, 58, color, !color);
          //ssd1306_send_data(&display); // Envia os dados para atualizar o display 
          //seta3();
          break;
       }
       *estado_led = (*estado_led + 1) % 3; // Alterna entre 0, 1 e 2
}

// Callback chamado quando ocorre interrupção em algum botão
void botao_callback(uint gpio, uint32_t eventos) {
    // Obtém o tempo atual em us
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) { // 200 ms
        //printf("led verde = %d\n", led_green);
        last_time = current_time; // Atualiza o tempo do último evento
        if (gpio == BOTAO_VERDE) { //  Botão A foi pressionado
          gpio_put(LED_VERDE, !gpio_get(LED_VERDE)); // Alterna o LED Verde 
        } else if (gpio == BOTAO_ALTERNAR) { //  Botão A foi pressionado
          alternar_leds(&estado_led);
        }
    }
}

void init_buttons() {
    // Configura os pinos dos botões como entrada com pull-up
    gpio_init(BOTAO_VERDE);
    gpio_init(BOTAO_ALTERNAR);
    gpio_set_dir(BOTAO_VERDE, GPIO_IN);
    gpio_set_dir(BOTAO_ALTERNAR, GPIO_IN);
    gpio_pull_up(BOTAO_VERDE); // Habilita pull-up no pino 5
    gpio_pull_up(BOTAO_ALTERNAR); // Habilita pull-up no pino 6
    gpio_set_irq_enabled_with_callback(BOTAO_VERDE, GPIO_IRQ_EDGE_FALL, true, botao_callback);
    gpio_set_irq_enabled_with_callback(BOTAO_ALTERNAR, GPIO_IRQ_EDGE_FALL, true, botao_callback);
}

// Função para converter as posições (x, y) da matriz para um índice do vetor de LEDs
int get_led_index(int x, int y) {
    if (x % 2 == 0) { // Se a linha for par
        return 24 - (x * 5 + y); // Calcula o índice do LED considerando a ordem da matriz
    } else { // Se a linha for ímpar
        return 24 - (x * 5 + (4 - y)); // Calcula o índice invertendo a posição dos LEDs
}}

// Definição da estrutura de cor para cada LED
struct pixel_t {
    uint8_t red, green, blue; // Componentes de cor: vermelho, verde e azul
};
typedef struct pixel_t led_t; // Cria um tipo led_t baseado em pixel_t
led_t leds[NUM_LEDS]; // Vetor que armazena as cores de todos os LEDs

// PIO e state machine para controle dos LEDs
PIO pio; // PIO utilizado para comunicação com os LEDs
uint state_machine;   // State machine associada ao PIO

// Função para atualizar os LEDs da matriz
void update_leds() {
    for (uint i = 0; i < NUM_LEDS; ++i) { // Percorre todos os LEDs
        pio_sm_put_blocking(pio, state_machine, leds[i].red); // Envia valor do componente vermelho
        pio_sm_put_blocking(pio, state_machine, leds[i].green); // Envia valor do componente verde
        pio_sm_put_blocking(pio, state_machine, leds[i].blue); // Envia valor do componente azul
    }
    sleep_us(100); // Aguarda 100 microsegundos para estabilizar
}
// Função de controle inicial da matriz de LEDs
void init_matrix(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program); // Carrega programa para controlar LEDs no PIO
    pio = pio0; // Seleciona o PIO 0
    state_machine = pio_claim_unused_sm(pio, true); // Reivindica uma state machine livre
    ws2818b_program_init(pio, state_machine, offset, pin, 800000.f); // Inicializa o controle da matriz no pino especificado

    for (uint i = 0; i < NUM_LEDS; ++i) { // Inicializa todos os LEDs com a cor preta (desligados)
        leds[i].red = leds[i].green = leds[i].blue = 0;
    }
    update_leds(); // Atualiza o estado dos LEDs
}
// Função para configurar a cor de um LED específico
void set_led_color(const uint index, const uint8_t red, const uint8_t green, const uint8_t blue) {
    leds[index].red = red; // Define o componente vermelho
    leds[index].green = green; // Define o componente verde
    leds[index].blue = blue; // Define o componente azul
}
// Função para desligar todos os LEDs
void turn_off_leds() {
    for (uint i = 0; i < NUM_LEDS; ++i) { // Percorre todos os LEDs
        set_led_color(i, 0, 0, 0); // Define a cor preta (desligado) para cada LED
    }
    update_leds();
}

// Desenhar os números na matriz de LEDs
void seta1() {  // Função para desenhar o número 0 na matriz de LEDs
    int matrix[5][5][3] = {  // Matriz tridimensional para representar a cor de cada LED (5x5)
        // Cada elemento [linha][coluna][RGB] define a cor de um LED
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}},   
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255},  {0, 0, 0}},   
        {{0, 0, 255}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255},  {0, 0, 255}},    
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255},  {0, 0, 0}},    
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}}    
    };
    // Loop para percorrer cada linha da matriz
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {  // Loop para percorrer cada coluna
            int position = get_led_index(row, col);  // Converte a posição da matriz para índice do vetor de LEDs
            // Define a cor do LED na posição correspondente
            set_led_color(position, matrix[row][col][0], matrix[row][col][1], matrix[row][col][2]);
        }}  update_leds();
}
void seta2() {
    int matrix[5][5][3] = {
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 255}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 255}},
        {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int position = get_led_index(row, col);
            set_led_color(position, matrix[row][col][0], matrix[row][col][1], matrix[row][col][2]);
        }}  update_leds();
}
void seta3() {
    int matrix[5][5][3] = {
        {{0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 0}, {0, 0, 255}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 255}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 255}, {0, 0, 255}, {0, 0, 255}}
    };
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int position = get_led_index(row, col);
            set_led_color(position, matrix[row][col][0], matrix[row][col][1], matrix[row][col][2]);
        }}  update_leds();
}

// Variável para controlar o estado do semáforo
typedef enum {
    Seta1,
    Seta2,
    Seta3
} Animacao;

Animacao Estado = Seta1;

// Função de callback do temporizador
bool repeating_timer_callback(struct repeating_timer *t) {


    // Muda o estado do semáforo
    switch (Estado) {
        case Seta1:
            seta1();
            Estado = Seta2;
            printf("estado/n");
            break;
        case Seta2:
            seta2();
            Estado = Seta3;
            printf("estado2/n");
            break;
        case Seta3:
            seta3();
            Estado = Seta1;
            printf("estado3/n");
            break;
    }

    return true; // Continua o temporizador
}

int main() {
    init_matrix(MATRIX_PIN);// Configura controle na matriz
    // Inicializa a biblioteca padrão da Pico
    stdio_init_all();
    init_leds();
    init_buttons();
    init_display();        // Inicializa o display
    bool color = true; // Declarado corretamente antes de seu uso
    int contador = 0;
    char texto[16];

     // Configura o temporizador para 3 segundos
    struct repeating_timer timer;
    add_repeating_timer_ms(3000, repeating_timer_callback, NULL, &timer);

    // Exibição inicial no display OLED
    ssd1306_fill(&display, !color);                             // Limpa o display preenchendo com a cor oposta ao valor atual de "color"
    ssd1306_rect(&display, 3, 3, 122, 58, color, !color);        // Desenha um retângulo com bordas dentro das coordenadas especificadas
    ssd1306_send_data(&display);                         // Envia os dados para atualizar o display

    // Desliga todos os LEDs inicialmente
    gpio_put(LED_VERDE, 0);
    gpio_put(LED_AZUL, 0);
    gpio_put(LED_VERMELHO, 0);

    while (true) {

      fflush(stdout); // Certifica-se de que o buffer de saída seja limpo antes de aguardar a entrada
      ssd1306_fill(&display, !color);                             // Limpa o display preenchendo com a cor oposta ao valor atual de "color"
      ssd1306_rect(&display, 3, 3, 122, 58, color, !color); 

      if (gpio_get(LED_VERDE)){
        ssd1306_draw_string(&display, "MM On", 10, 10);
        //ssd1306_send_data(&display); // Envia os dados para atualizar o display
      } else {
        snprintf(texto, sizeof(texto), "%d km|h", contador);
        ssd1306_draw_string(&display, texto, 45, 25);
      }

      ssd1306_send_data(&display);  // Atualiza o display

      if (gpio_get(LED_AZUL)){
        ssd1306_draw_string(&display, "Gas 5L", 10, 50);
        //ssd1306_send_data(&display); // Envia os dados para atualizar o display
      } else if (gpio_get(LED_VERMELHO)){
        ssd1306_draw_string(&display, "Gas 2L", 10, 50);
        //ssd1306_send_data(&display); // Envia os dados para atualizar o display
      } else {
        //ssd1306_fill(&display, !color);
        ssd1306_rect(&display, 3, 3, 122, 58, color, !color);
        //ssd1306_send_data(&display); // Envia os dados para atualizar o display
      }
   
      ssd1306_send_data(&display);  // Atualiza o display
                      
      contador++;
      if (contador > 100) {  
        contador = 0;                
      }
          sleep_ms(500);
    }
}