#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include <stdio.h>
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "lib/matriz_leds.h"
#include "task.h"
#include "queue.h"

//Estrutura do display
ssd1306_t ssd;

// --- DEFINICOES ---
#define ADC_AGUA 27   // ADC1 - Nivel de Agua
#define ADC_CHUVA 26  // ADC0 - Volume de Chuva
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12
#define BUZZER_PIN 21
#define WS2812_PIN 7    //Pino do WS2812
#define I2C_SDA 14      //Pino SDA - Dados
#define I2C_SCL 15      //Pino SCL - Clock
#define IS_RGBW false   //Maquina PIO para RGBW
#define BOTAO_A 5

//Função para modularizar a inicialização do hardware
void inicializar_componentes(){
    stdio_init_all();
    //Inicializa o botão
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    // Inicializa LED Vermelho
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_RED, 0);
    // Inicializa LED Verde
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_put(LED_GREEN, 0);
    // Inicializa LED Azul
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_BLUE, 0);

    //Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    
    //Inicializa ADC para leitura do Joystick
    adc_init();
    adc_gpio_init(ADC_CHUVA);
    adc_gpio_init(ADC_AGUA);

    //Inicializa buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);   //Slice para o buzzer
    float clkdiv = 125.0f; // Clock divisor
    uint16_t wrap = (uint16_t)((125000000 / (clkdiv * 1000)) - 1);      //Valor do Wrap
    pwm_set_clkdiv(buzzer_slice, clkdiv);       //Define o clock
    pwm_set_wrap(buzzer_slice, wrap);           //Define o wrap
    pwm_set_gpio_level(BUZZER_PIN, wrap * 0.3f); //Define duty
    pwm_set_enabled(buzzer_slice, false); //Começa desligado

    //Inicializa I2C para o display SSD1306
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  //Dados
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);  //Clock
    //Define como resistor de pull-up interno
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicializa display
    ssd1306_init(&ssd, 128, 64, false, 0x3C, i2c1);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}


//Estrutura para armazenar leitura dos sensores (nivel da agua e volume da chuva)
typedef struct{
    uint8_t nivel_agua;
    uint8_t volume_chuva;
}LeituraSensor;

//Variaveis globais
QueueHandle_t filaSensores;
bool em_alerta = false;
uint buzzer_slice;

//Funcao para converter o valor lido do ADC para porcentagem
int map_adc(uint16_t val){
    return (val * 100) / 4095;  //0 a 100 em porcentagem
}

//Task para leitura dos sensores via ADC
void vTaskLeituraJoystick(void *param){
    while(true){
        adc_select_input(1);
        uint16_t adc_agua = adc_read();
        adc_select_input(0);
        uint16_t adc_chuva = adc_read();

        LeituraSensor leitura = {
            .nivel_agua = map_adc(adc_agua),
            .volume_chuva = map_adc(adc_chuva)
        };

        xQueueSend(filaSensores, &leitura, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//Task para processamento dos dados lidos
void vTaskProcessamento(void *param){
    LeituraSensor dados;
    while(true){
        if(xQueueReceive(filaSensores, &dados, portMAX_DELAY)){
            em_alerta = (dados.nivel_agua >= 70 || dados.volume_chuva >= 80);
            // (Aqui pode-se enviar para outras filas, se quiser)
        }
    }
}

//Task para exibicao dos dados lidos
void vTaskDisplay(void *param){
    while(true){
        LeituraSensor leitura;
        if (xQueuePeek(filaSensores, &leitura, 0)) {
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, em_alerta ? "!! ALERTA !!" : "Monitoramento", 10, 0);
            char buffer[32];
            sprintf(buffer, "Agua: %d%%", leitura.nivel_agua);
            ssd1306_draw_string(&ssd, buffer, 10, 20);
            sprintf(buffer, "Chuva: %d%%", leitura.volume_chuva);
            ssd1306_draw_string(&ssd, buffer, 10, 40);
            ssd1306_send_data(&ssd);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void atualizar_rgb(uint8_t nivel){
    if(nivel >= 70){
        gpio_put(LED_RED, 1);
        gpio_put(LED_GREEN, 0);
    }else if (nivel >= 60){
        gpio_put(LED_RED, 1);
        gpio_put(LED_GREEN, 1);
    }else{
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 1);
    }
}

//Task para visualizacao dos leds
void vTaskLedRGB(void *param){
    atualizar_rgb(0);
}

//Task para tocar o buzzer
void vTaskBuzzer(void *param){
    while(true){
        if(em_alerta){
            for(int i = 0; i < 3; i++){
                pwm_set_enabled(buzzer_slice, true);
                vTaskDelay(pdMS_TO_TICKS(150));
                pwm_set_enabled(buzzer_slice, false);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelay(pdMS_TO_TICKS(500));  // pausa entre ciclos
        } else {
            pwm_set_enabled(buzzer_slice, true);
            vTaskDelay(pdMS_TO_TICKS(400));
            pwm_set_enabled(buzzer_slice, false);
            vTaskDelay(pdMS_TO_TICKS(600));
        }
    }
}

// Converte a porcentagem de nível de água em um índice de exibição
int calcular_nivel_visual(uint8_t nivel){
    if (nivel <= 20) return 1;
    else if (nivel <= 40) return 2;
    else if (nivel <= 60) return 3;
    else if (nivel <= 80) return 4;
    else return 5;
}

void vTaskMatrizLeds(void *param){
    LeituraSensor leitura;

    while(true){
        if (xQueuePeek(filaSensores, &leitura, 0)){
            if(em_alerta){
                set_one_led(255, 255, 0, 0);  // Exclamação
            }else{
                int nivel = calcular_nivel_visual(leitura.nivel_agua);
                set_one_led(0, 0, 255, nivel);  // Azul para níveis
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Atualiza a cada meio segundo
    }
}

void vTaskBotao(void *param){
    static bool ultimo_estado = true; // estado anterior do botão
    while(true){
        bool estado_atual = gpio_get(BOTAO_A);
        
        if (!estado_atual && ultimo_estado) { // botão pressionado (falling edge)
            em_alerta = !em_alerta; // alterna manualmente o modo de alerta
            printf(">>> Modo alerta %s manualmente!\n", em_alerta ? "ATIVADO" : "DESATIVADO");
            vTaskDelay(pdMS_TO_TICKS(300)); // debounce + evitar múltiplas detecções
        }

        ultimo_estado = estado_atual;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


int main(){
    stdio_init_all();
    inicializar_componentes();

    // Fila
    filaSensores = xQueueCreate(5, sizeof(LeituraSensor));

    // Tarefas
    xTaskCreate(vTaskLeituraJoystick, "Leitura", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskProcessamento, "Processa", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskDisplay, "Display", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskLedRGB, "LedRGB", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskBuzzer, "Buzzer", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskMatrizLeds, "MatrizLEDs", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskBotao, "BotaoA", 1024, NULL, 1, NULL);

    vTaskStartScheduler();
}