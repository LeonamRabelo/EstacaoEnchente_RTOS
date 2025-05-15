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

//Função para modularizar a inicialização do hardware
void inicializar_componentes(){
    stdio_init_all();
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
        printf("Nivel Agua: %d%% | Chuva: %d%% | Status: %s\n",
               em_alerta ? 70 : 30,
               em_alerta ? 90 : 20,
               em_alerta ? "ALERTA" : "Normal");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//Task para visualizacao dos leds
void vTaskVisual(void *param){
    while(true){
        gpio_put(LED_RED, em_alerta);
        gpio_put(LED_GREEN, !em_alerta);
        gpio_put(LED_BLUE, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

//Task para tocar o buzzer
void vTaskBuzzer(void *param){
    while(true){
        if(em_alerta){
        pwm_set_enabled(buzzer_slice, true);
        sleep_ms(200);
        pwm_set_enabled(buzzer_slice, false);
        sleep_ms(800);
        }else{
            vTaskDelay(pdMS_TO_TICKS(300));
        }
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
    xTaskCreate(vTaskVisual, "Visual", 1024, NULL, 1, NULL);
    xTaskCreate(vTaskBuzzer, "Buzzer", 1024, NULL, 1, NULL);

    vTaskStartScheduler();
}