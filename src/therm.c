#include "therm.h"
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"  

esp_err_t therm_config(therm_t *thermistor, adc_oneshot_unit_handle_t adc, adc_channel_t channel, gpio_num_t gpio_pin) {
    thermistor->adc_hdlr = adc;
    thermistor->adc_channel = channel;
    thermistor->gpio_pin = gpio_pin;
    

    if (gpio_pin != GPIO_NUM_NC  ){

        gpio_set_direction(thermistor->gpio_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(thermistor->gpio_pin, 0);

    }
    

    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11, // Rango de 0 a 3.3V
        .bitwidth = ADC_BITWIDTH_12, // Resolución de 12 bits
    };

    return adc_oneshot_config_channel(thermistor->adc_hdlr, thermistor->adc_channel, &channel_cfg);
}

float therm_read_v(therm_t thermistor) {
    uint16_t lsb = therm_read_lsb(thermistor);
    return _therm_lsb2v(lsb);
}

uint16_t therm_read_lsb(therm_t thermistor) {
    int raw_value = 0;
    
    if (thermistor.gpio_pin != GPIO_NUM_NC) gpio_set_level(thermistor.gpio_pin, 1);

    ESP_ERROR_CHECK(adc_oneshot_read(thermistor.adc_hdlr, thermistor.adc_channel, &raw_value));

     if (thermistor.gpio_pin != GPIO_NUM_NC) gpio_set_level(thermistor.gpio_pin, 0);


    return (uint16_t)raw_value;
}

float therm_read_t(therm_t thermistor) {
    float voltage = therm_read_v(thermistor);
    return _therm_v2t(voltage);
}

float _therm_v2t(float v) {
    float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;
    if(r_ntc <0){
         r_ntc *= -1;
    }
   
    float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + log(r_ntc / NOMINAL_RESISTANCE) / BETA_COEFFICIENT);
    return t_kelvin - 273.15f;
}

float _therm_lsb2v(uint16_t lsb) {
    return (float)lsb * 3.3f / 4095.0f;
}
