#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "matriz_leds.h"

//Função para ligar um LED
static inline void put_pixel(uint32_t pixel_grb){
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

//Função para converter cores RGB para um valor de 32 bits
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b){
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

bool alertas[1][NUM_PIXELS] = {
//Amarelo (Exclamação de atenção)
{
    0, 0, 1, 0, 0,      
    0, 0, 0, 0, 0, 
    0, 0, 1, 0, 0,   
    0, 0, 1, 0, 0,  
    0, 0, 1, 0, 0
},
//Apagar tudo
{
    0, 0, 0, 0, 0,      
    0, 0, 0, 0, 0,  
    0, 0, 0, 0, 0,   
    0, 0, 0, 0, 0,  
    0, 0, 0, 0, 0
}
};

//Função para envio dos dados para a matriz de leds
void set_one_led(uint8_t r, uint8_t g, uint8_t b, int numero){
    //Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(255, 255, 0);

    //Define todos os LEDs com a cor especificada
    for(int i = 0; i < NUM_PIXELS; i++){
        if(alertas[numero][i]){     //Chama a matriz de leds com base no numero passado
            put_pixel(color);           //Liga o LED com um no buffer
        }else{
            put_pixel(0);               //Desliga os LEDs com zero no buffer
        }
    }
}