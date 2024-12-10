/**********************************************************************
* FILENAME : task_sensor.c       
*
* DESCRIPTION : 
*
* PUBLIC LICENSE :
* Este código es de uso público y libre de modificar bajo los términos de la
* Licencia Pública General GNU (GPL v3) o posterior. Se proporciona "tal cual",
* sin garantías de ningún tipo.
*
* AUTHOR :   Dr. Fernando Leon (fernando.leon@uco.es) University of Cordoba
******************************************************************************/


/*
	Circuito del termistor 1. 

   3.3V
     |
     |
  [ NTC ]  <-- Termistor 10K 
     |
     |-----------> ADC IN (GPIO34 por defecto. Ver config.h)
     |
  [ 10K ]  <-- R fija 10K
     |
    GND
**/

// libc 
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include "therm.h"
// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// espidf
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_adc/adc_oneshot.h>

// propias
#include "config.h"

static const char *TAG = "STF_P1:task_sensor";

// esta tarea temporiza la lectura de un termistor. 
// Para implementar el periodo de muestreo, se utiliza un semáforo declarado de forma global 
// para poder ser utilizado desde esta rutina de expiración del timer.
// Cada vez que el temporizador expira, libera el semáforo
// para que la tarea realice una iteración. 
static SemaphoreHandle_t semSample = NULL;
static void tmrSampleCallback(void* arg)
{
	xSemaphoreGive(semSample);
}

// ESP32 tiene unidad de conversión analógica digital, que debe configurarse 
// previamente a las lecturas mediante esta estructura manejador. Se define global
// porque únicamente usaremos un ADC. Se configura en la tarea antes del loop
adc_oneshot_unit_handle_t adc_hdlr; 


// Funciones de lectura del sensor.

// Se trata de un termistor, un sensor analógico activo (require alimentación)
// cuya salida es voltaje. El circuito está en la cabecera de este fichero. 
//
// Pasos para adquirir un valor: 
// 1) El ADC del SoC devuelve un valor binario crudo (LSB), que corresponde a un valor
// de voltaje en la entrada del canal.

// 2) Convertir este valor en voltaje. Para esto, es necesario conocer el número 
// de bits que tiene el ADC (para conocer el máximo valor binario que
// se puede obtener) y el voltaje de referencia (el máximo voltaje que es 
// capaz de medir)

// LSB crudo
// int read_adc() 
// {
//     int raw_value = 0;
//     ESP_ERROR_CHECK(adc_oneshot_read(adc_hdlr, THERMISTOR_ADC_CHANNEL, &raw_value));
//     return raw_value;
// }

// LSB -> V
// 12 bits -> 2^12 = 4096 -> rango de 0 a 4095. 
// V de referencia interno del ESP32 -> 3.3V
//#define lsb_to_v(x) (float) ((x) * 3.3f / 4095.0f)

// V -> Temperatura en C
// float v_to_temperature(float v)
// {
// 	// resistencia del termistor, obtenida por el voltaje medido en el adc. 
//     float r_ntc = SERIES_RESISTANCE * (3.3 - v) / v;

// 	// Ecuación de Steinhart-Hart, que relaciona la resistencia que ofrece un material semiconductor 
// 	// con la variación de la temperatura en Kelvin, de acuerdo a unos coeficientes que caracterizan
// 	// al semiconductor en cuestión (están definidos en config.h) 
// 	float t_kelvin = 1.0f / (1.0f / NOMINAL_TEMPERATURE + (1.0f / BETA_COEFFICIENT) * log(r_ntc / NOMINAL_RESISTANCE));

// 	// Resultado en grados centígrados
//     return(t_kelvin - 273.15f);  
// }

// Tarea SENSOR
SYSTEM_TASK(TASK_SENSOR)
{	
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Sensor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_sensor_args_t* ptr_args = (task_sensor_args_t*) TASK_ARGS;
	RingbufHandle_t* rbuf = ptr_args->rbuf;
	RingbufHandle_t* cbuf = ptr_args->cbuf; 
	uint8_t frequency = ptr_args->freq;
	uint8_t n = ptr_args->n;
	uint64_t period_us = 1000000 / frequency;

	// Estructura de datos para configurar la unidad ADC
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = THERMISTOR_ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };

	// Establecimiento de la configuracón en ADC
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_hdlr));
	// Configura el termistor
	therm_t termistor;
	ESP_ERROR_CHECK(therm_config(&termistor, adc_hdlr, THERMISTOR_ADC_CHANNEL, THERMISTOR_ADC_CHANNEL_ON_OFF));
	therm_t termistor2;
	ESP_ERROR_CHECK(therm_config(&termistor2, adc_hdlr, THERMISTOR_ADC_CHANNEL2, THERMISTOR2_ADC_CHANNEL_ON_OFF));

	// Estructura de datos para configurar el canal de la unidad ADC
    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_11, // Rango de 0 a 3.3V
        .bitwidth = ADC_BITWIDTH_12, // Resolución de 12 bits
    };
	// Establecimiento de la configuración del canal
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_hdlr, THERMISTOR_ADC_CHANNEL, &channel_cfg));
	ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_hdlr, THERMISTOR_ADC_CHANNEL2, &channel_cfg));


	// Inicializa el semáforo (la estructura del manejador se definió globalmente)
	semSample = xSemaphoreCreateBinary();

	// Crea y establece una estructura de configuración para el temporizador
	const esp_timer_create_args_t tmrSampleArgs = {
		.callback = &tmrSampleCallback,
		.name = "Timer Configuration"
	};

	// Lanza el temporizador, conel periodo de muestreo recibido como parámetro
	esp_timer_handle_t tmrSample;
	ESP_ERROR_CHECK(esp_timer_create(&tmrSampleArgs, &tmrSample));
	ESP_ERROR_CHECK(esp_timer_start_periodic(tmrSample, period_us));
	
	// variables para reutilizar en el bucle
	void *ptr;
	void* ptr2;
	data_item_t items[2];
	items[0].source=0;
	items[1].source=0;

 // Vector para almacenar las dos lecturas
	int i = 0;
	// Loop
	TASK_LOOP()
	{
		// Se bloquea a la espera del semáforo. Si el periodo establecido se retrasa un 20%
		// el sistema se reinicia por seguridad. Este mecanismo de watchdog software es útil
		// en tareas periódicas cuyo periodo es conocido. 
		if(xSemaphoreTake(semSample, ((1000/frequency)*1.2)/portTICK_PERIOD_MS))
		{	
			// lectura del sensor. Se obtiene el valor en grados centígrados
			items[0].value = therm_read_t(termistor);
			ESP_LOGI(TAG, "valor medido (pre buffer 1): %.4f", items[0].value);
			++i;

			if (xRingbufferSendAcquire(*rbuf, &ptr, sizeof(data_item_t), pdMS_TO_TICKS(100)) != pdTRUE)
			{
					// Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
					// una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
					ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf));
			}
			else 
			{
				memcpy(ptr,&items,sizeof(data_item_t));
				xRingbufferSendComplete(*rbuf, ptr);
			}
			
			if( i%n == 0){
				items[1].value = therm_read_t(termistor2);
				ESP_LOGI(TAG, "valor medido (pre buffer 2): %.4f", items[1].value);

				// Uso del buffer cíclico entre la tarea monitor y sensor. Ver documentación en ESP-IDF
				// Pide al RingBuffer espacio para escribir un float. 
				if (xRingbufferSendAcquire(*cbuf, &ptr, 2*sizeof(data_item_t), pdMS_TO_TICKS(100)) != pdTRUE)
				{
					// Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
					// una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
					ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*cbuf));
				}
				else 
				{
					// Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
					// en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que 
					// se notifique que se ha completado la escritura
					memcpy(ptr,items,2*sizeof(data_item_t));

					// Se notifica que la escritura ha completado. 
					xRingbufferSendComplete(*cbuf, ptr);
				}

			}
			


			
		}
		else
		{
			ESP_LOGI(TAG,"Watchdog (soft) failed");
			esp_restart();
		}
	}
	
	ESP_LOGI(TAG,"Deteniendo la tarea...");
	// detención controlada de las estructuras que ha levantado la tarea
	ESP_ERROR_CHECK(esp_timer_stop(tmrSample));
	ESP_ERROR_CHECK(esp_timer_delete(tmrSample));
	TASK_END();
}