/**********************************************************************
* FILENAME : task_monitor.c       
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

// libc
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

// freerqtos
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// esp
#include <esp_system.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>

// propias
#include "config.h"
#include "therm.h"
static const char *TAG = "STF_P1:task_monitor";


// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Monitor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
	RingbufHandle_t* rbuf = ptr_args->rbuf; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;
	float v;
	data_item_t* received_item[3];
	data_item_t* media;
	// Loop
	TASK_LOOP()
	{
		// Se bloquea en espera de que haya algo que leer en RingBuffer.
		// Tiene un timeout de 1 segundo para no bloquear indefinidamente la tarea, 
		// pero si expira vuelve aquí sin consecuencias
		ptr = xRingbufferReceive(*rbuf, &length, pdMS_TO_TICKS(1000));

		
				//Si el timeout expira, este puntero es NULL
				if (ptr != NULL && length == 3*sizeof(data_item_t)) {
					received_item[0]  = (data_item_t *) ptr;
					received_item[1]  = (data_item_t *) ptr + 1;
					received_item[2]  = (data_item_t *) ptr + 2;

					if (received_item[0]->source == 0) {
						// Origen TASK_SENSOR
						// ESP_LOGI(TAG, "NORMAL_MODE:T = {%.4f} ºC", received_item[0]->value);
						// ESP_LOGI(TAG, "NORMAL_MODE:T = {%.4f} ºC", received_item[1]->value);
						// ESP_LOGI(TAG, "NORMAL_MODE:T = {%.4f} ºC", received_item[2]->value);
				
					}
					
					vRingbufferReturnItem(*rbuf, ptr);
				}else if (ptr != NULL && length == sizeof(data_item_t)){

					media = (data_item_t *) ptr;
					float res = _therm_v2t(_therm_lsb2v(media->value));

					if (media->source == 2) {
						ESP_LOGI(TAG, "NORMAL_MODE:Media = {%.4f} ºC", res);
					}
					vRingbufferReturnItem(*rbuf, ptr);
				}
				else{
					ESP_LOGW(TAG, "Esperando datos ...");
				}
			

			

		
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
