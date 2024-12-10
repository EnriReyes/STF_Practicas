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
#include <string.h>

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

static const char *TAG = "STF_P1:task_check";


// Tarea MONITOR
SYSTEM_TASK(TASK_CHECK)
{
	TASK_BEGIN();
	ESP_LOGI(TAG,"Task Check running");

	// Recibe los argumentos de configuración de la tarea y los desempaqueta
	task_check_args_t* ptr_args = (task_check_args_t*) TASK_ARGS;
	RingbufHandle_t* cbuf = ptr_args->cbuf; 
    RingbufHandle_t* rbuf = ptr_args->rbuf; 

	// variables para reutilizar en el bucle
	size_t length;
	void *ptr;
	float v[2];
    void* ptr2 ;
	data_item_t diferencia;
	diferencia.source = 1;
	data_item_t* item1;
	data_item_t* item2;

	// Loop
	TASK_LOOP()
	{
		// Se bloquea en espera de que haya algo que leer en RingBuffer.
		// Tiene un timeout de 1 segundo para no bloquear indefinidamente la tarea, 
		// pero si expira vuelve aquí sin consecuencias
		ptr = xRingbufferReceive(*cbuf, &length, pdMS_TO_TICKS(1000));

		//Si el timeout expira, este puntero es NULL
		if (ptr != NULL) 
		{
			// Este código se puede usar para notificar cuántos bytes ha recibido del
			// sensor a través de la estructura RingBuffer. 
			//ESP_LOGI(TAG,"Recibidos: %d bytes", length);
			item1 = ((data_item_t *) ptr);
			item2 = (((data_item_t *) ptr+1));
			diferencia.value = ((item1->value-item2->value)/item2->value)*100;
			if (diferencia.value <0) diferencia.value *= -1;
			

			

			vRingbufferReturnItem(*cbuf, ptr);

            if (xRingbufferSendAcquire(*rbuf, &ptr2, sizeof(data_item_t), pdMS_TO_TICKS(100)) != pdTRUE)
			{
					// Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
					// una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
					ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf));
			}
			else 
			{
					// Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
					// en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que 
					// se notifique que se ha completado la escritura
					memcpy(ptr2,&diferencia,sizeof(data_item_t));
                    
					// Se notifica que la escritura ha completado. 
					xRingbufferSendComplete(*rbuf, ptr2);
			}
		} 
		else 
		{
			ESP_LOGW(TAG, "Esperando datos ...");
		}
	}
	ESP_LOGI(TAG,"Deteniendo la tarea ...");
	TASK_END();
}
