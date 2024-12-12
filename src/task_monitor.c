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

static const char *TAG = "STF_P1:task_monitor";


// Tarea MONITOR
SYSTEM_TASK(TASK_MONITOR)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Monitor running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_monitor_args_t* ptr_args = (task_monitor_args_t*) TASK_ARGS;
	RingbufHandle_t* rbuf = ptr_args->rbuf; 
	system_t* sys_stf_p1  = ptr_args->sys_stf_p1;
	system_task_t* task_monitor = ptr_args->task_monitor;

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;
	float v;
	float storage_item = 0;
	data_item_t* received_item;
	// Loop
	TASK_LOOP()
	{
		// Se bloquea en espera de que haya algo que leer en RingBuffer.
		// Tiene un timeout de 1 segundo para no bloquear indefinidamente la tarea, 
		// pero si expira vuelve aquí sin consecuencias
		ptr = xRingbufferReceive(*rbuf, &length, pdMS_TO_TICKS(1000));

		switch(GET_ST_FROM_TASK())
 {
			case NORMAL_MODE:
				//Si el timeout expira, este puntero es NULL
				if (ptr != NULL && length == sizeof(data_item_t)) {
					received_item = (data_item_t *) ptr;
					if (received_item->source == 0) {
						// Origen TASK_SENSOR
						ESP_LOGI(TAG, "NORMAL_MODE:T = {%.4f} ºC", received_item->value);
						
					} else if (received_item->source == 1) {
						// Origen TASK_CHECK	
						storage_item = received_item->value;
					}
					vRingbufferReturnItem(*rbuf, ptr);
				}else 
				{
					ESP_LOGW(TAG, "Esperando datos ...");
				}
			break;

			case DEGRADED_MODE:
				if (ptr != NULL && length == sizeof(data_item_t)) {
					received_item = (data_item_t *) ptr;
					if (received_item->source == 0) {
						// Origen TASK_SENSOR
						float res1 = (received_item->value-(storage_item/100)*received_item->value);
						float res2 = (received_item->value+(storage_item/100)*received_item->value);
						ESP_LOGI(TAG, "DEGRADED_MODE:T = (%.4f - %.4f) ºC", res1, res2);
						
					} else if (received_item->source == 1) {
						// Origen TASK_CHECK	
						storage_item = received_item->value;
					}
					vRingbufferReturnItem(*rbuf, ptr);
				}else 
				{
					ESP_LOGW(TAG, "Esperando datos ...");
				}
			break;

			case ERROR:
					ESP_LOGI(TAG, "Sensor ERROR. Repare and restart.");
					system_task_stop(sys_stf_p1, task_monitor, TASK_SENSOR_TIMEOUT_MS);
			break;
			default:
				vTaskDelay(100);
				// esta condición puede ocurrir al entrar
				// la primera vez desde el estado INIT,
				// ya que antes de transitar a NORMAL_MODE
				// se crea la tarea Monitor
 		}

		
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
