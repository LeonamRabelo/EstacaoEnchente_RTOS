#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include <stdio.h>
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "lib/matriz_leds.h"
#include "task.h"
#include "queue.h"

//Estrutura do display
ssd1306_t ssd;

//Pinagens de hardware
#define ADC_AGUA 27   // ADC1 - Nivel de Agua
#define ADC_CHUVA 26  // ADC0 - Volume de Chuva
#define LED_RED 13      //LED Vermelho
#define LED_GREEN 11    //LED Verde
#define LED_BLUE 12     //LED Azul
#define BUZZER_PIN 21   //Pino do Buzzer
#define WS2812_PIN 7    //Pino do WS2812
#define I2C_SDA 14      //Pino SDA - Dados
#define I2C_SCL 15      //Pino SCL - Clock
#define IS_RGBW false   //Maquina PIO para RGBW
#define BOTAO_A 5       //Botão A

//Estrutura para armazenar leitura dos sensores (nivel da agua e volume da chuva)
typedef struct{
    uint8_t nivel_agua;
    uint8_t volume_chuva;
}LeituraSensor;

//Fila para armazenar leitura dos sensores
QueueHandle_t xFilaDisplay;
QueueHandle_t xFilaMatrizLeds;
QueueHandle_t xFilaLedRGB;

//Variaveis globais
bool em_alerta = false;
uint buzzer_slice;
bool alerta_manual = false;

//Funcao para converter o valor lido do ADC para porcentagem
int map_adc(uint16_t val){
    return (val * 100) / 4095;  //0 a 100 em porcentagem
}
//Task para leitura dos sensores via ADC
void vTaskLeituraJoystick(void *param){
    //Inicializa ADC para leitura do Joystick
    adc_init();
    adc_gpio_init(ADC_CHUVA);
    adc_gpio_init(ADC_AGUA);

    while(true){
        //Leitura dos ADCs
        adc_select_input(1);
        uint16_t adc_agua = adc_read();
        adc_select_input(0);
        uint16_t adc_chuva = adc_read();

        //Conversão para porcentagem
        uint8_t nivel_agua = map_adc(adc_agua);
        uint8_t volume_chuva = map_adc(adc_chuva);

        if(!alerta_manual){  //Verifica se o botao foi pressionado para ativar o alerta manualmente
            em_alerta = (nivel_agua >= 70 || volume_chuva >= 80);
        }

        // Pode enviar para fila se necessário, por exemplo para o display
        LeituraSensor leitura = {
            .nivel_agua = nivel_agua,
            .volume_chuva = volume_chuva
        };
        //Envia para as filas de leitura
        xQueueSend(xFilaDisplay, &leitura, 0);
        xQueueSend(xFilaMatrizLeds, &leitura, 0);
        xQueueSend(xFilaLedRGB, &leitura, 0);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//Task para exibicao dos dados lidos
void vTaskDisplay(void *param){
    //Inicializa I2C para o display SSD1306
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  //Dados
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);  //Clock
    //Define como resistor de pull-up interno
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    //Inicializa display
    ssd1306_init(&ssd, 128, 64, false, 0x3C, i2c1);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while(true){
        LeituraSensor leitura;  //Leitura da fila
        if(xQueueReceive(xFilaDisplay, &leitura, 0)){   //Chama a leitura da fila
            ssd1306_fill(&ssd, false);                  //Limpa o display
            ssd1306_draw_string(&ssd, em_alerta ? "!! ALERTA !!" : "Monitoramento", 10, 0); //Exibe o texto no display de acordo com o alerta
            //Exibe os dados no display de acordo com os dados lidos
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

//Task para visualizacao dos leds
void vTaskLedRGB(void *param){
    //Inicializa LED Vermelho
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

    LeituraSensor leitura;
    while(true){
        if(xQueueReceive(xFilaLedRGB, &leitura, 0)){
            if(leitura.nivel_agua >= 70 || leitura.volume_chuva >= 80 || em_alerta){ //Valor de alerta, perigo
                gpio_put(LED_RED, 1);
                gpio_put(LED_GREEN, 0);
                }else if(leitura.nivel_agua >= 60 || leitura.volume_chuva >= 70){   //Valor proximo ao risco
                    gpio_put(LED_RED, 1);
                    gpio_put(LED_GREEN, 1);
                    }else{  //Nivel normal
                        gpio_put(LED_RED, 0);
                        gpio_put(LED_GREEN, 1);
                    }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//Task para tocar o buzzer
void vTaskBuzzer(void *param){
    //Inicializa buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);   //Slice para o buzzer
    float clkdiv = 125.0f; // Clock divisor
    uint16_t wrap = (uint16_t)((125000000 / (clkdiv * 1000)) - 1);      //Valor do Wrap
    pwm_set_clkdiv(buzzer_slice, clkdiv);       //Define o clock
    pwm_set_wrap(buzzer_slice, wrap);           //Define o wrap
    pwm_set_gpio_level(BUZZER_PIN, wrap * 0.3f); //Define duty
    pwm_set_enabled(buzzer_slice, false); //Começa desligado
    while(true){
        if(em_alerta){
            for(int i = 0; i < 3; i++){
                pwm_set_enabled(buzzer_slice, true);
                vTaskDelay(pdMS_TO_TICKS(150));
                pwm_set_enabled(buzzer_slice, false);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelay(pdMS_TO_TICKS(500));  // pausa entre ciclos
        }else{
            pwm_set_enabled(buzzer_slice, false);
            vTaskDelay(pdMS_TO_TICKS(500));  // pausa entre ciclos
        }
    }
}

// Converte a porcentagem de nível de água em um índice de exibição
int calcular_nivel_visual(uint8_t nivel){
    if(nivel <= 20) return 1;
    else if (nivel <= 40) return 2;
    else if (nivel <= 60) return 3;
    else if (nivel <= 80) return 4;
    else return 5;
}

void vTaskMatrizLeds(void *param){
    //Inicializa o pio
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    LeituraSensor leitura;
    while(true){
        if(xQueueReceive(xFilaMatrizLeds, &leitura, 0)){
            if(em_alerta){
                set_one_led(100, 0, 0, 0);  //Exclamação vermelha
            }else{
                int nivel = calcular_nivel_visual(leitura.nivel_agua);
                set_one_led(0, 0, 100, nivel);  //Azul para níveis normais, representando o nível de agua
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));  //Atualiza a cada meio segundo
    }
}

void vTaskBotao(void *param){
    //Inicializa o botão
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    static bool ultimo_estado = true; // estado anterior do botão
    while(true){
        bool estado_atual = gpio_get(BOTAO_A);
        
        if (!estado_atual && ultimo_estado) { // botão pressionado (falling edge)
            alerta_manual = !alerta_manual;
            em_alerta = alerta_manual ? true : false;
            printf(">>> Modo alerta %s manualmente!\n", em_alerta ? "ATIVADO" : "DESATIVADO");
            vTaskDelay(pdMS_TO_TICKS(300)); //debounce + evitar múltiplas detecções
        }

        ultimo_estado = estado_atual;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main(){
    stdio_init_all();

    // Fila
    xFilaDisplay = xQueueCreate(7, sizeof(LeituraSensor));
    xFilaMatrizLeds = xQueueCreate(7, sizeof(LeituraSensor));
    xFilaLedRGB = xQueueCreate(7, sizeof(LeituraSensor));

    //Tarefas
    xTaskCreate(vTaskLeituraJoystick, "Leitura", 256, NULL, 1, NULL);
    xTaskCreate(vTaskDisplay, "Display", 512, NULL, 1, NULL);
    xTaskCreate(vTaskLedRGB, "LedRGB", 256, NULL, 1, NULL);
    xTaskCreate(vTaskBuzzer, "Buzzer", 256, NULL, 1, NULL);
    xTaskCreate(vTaskMatrizLeds, "MatrizLEDs", 512, NULL, 1, NULL);
    xTaskCreate(vTaskBotao, "BotaoA", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}