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

static const char *TAG = "STF_P1:task_votador";

// Tarea MONITOR
SYSTEM_TASK(TASK_VOTADOR)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task Votador running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_votador_args_t* ptr_args = (task_votador_args_t*) TASK_ARGS;
    RingbufHandle_t* rbuf = ptr_args->rbuf; 
    RingbufHandle_t* vbuf = ptr_args->vbuf; 
	uint16_t mascara = ptr_args->mascara;

    // Variables para reutilizar en el bucle
    size_t length;
    void *ptr = NULL;
    void *ptr2 = NULL;

    data_item_t received_item[3];
    data_item_t media;

    // Inicializa valores por defecto
    memset(received_item, 0, sizeof(received_item));
    media.source = 2;
    media.value = 0.0f;

    // Loop
    TASK_LOOP()
    {
        // Bloquea hasta recibir datos del RingBuffer o que expire el timeout
        ptr = xRingbufferReceive(*vbuf, &length, pdMS_TO_TICKS(1000));

        if (ptr != NULL) {
            // Verifica el tamaño esperado del buffer recibido
            if (length == 3 * sizeof(data_item_t)) {
                // Copia los datos recibidos en el array local
                memcpy(received_item, ptr, length);

                // Procesa los datos si el origen es correcto
                if (received_item[0].source == 0) {
                    media.value = (received_item[0].value & received_item[1].value) | (received_item[0].value & received_item[2].value) | (received_item[1].value & received_item[2].value);
					media.value  = media.value & mascara;
                    // Intenta adquirir espacio en el RingBuffer de salida
                    if (xRingbufferSendAcquire(*rbuf, &ptr2, sizeof(data_item_t), pdMS_TO_TICKS(100)) != pdTRUE) {
                        ESP_LOGI(TAG, "Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*rbuf));
                    } else {
                        memcpy(ptr2, &media, sizeof(data_item_t));
                        xRingbufferSendComplete(*rbuf, ptr2);
                    }
                }

                // Devuelve el elemento al buffer de entrada
                vRingbufferReturnItem(*vbuf, ptr);
            } else {
                ESP_LOGE(TAG, "Tamaño inesperado: %zu", length);
                vRingbufferReturnItem(*vbuf, ptr);
            }
        } else {
            ESP_LOGW(TAG, "Esperando datos ...");
        }
    }

    ESP_LOGI(TAG, "Deteniendo la tarea ...");
    TASK_END();
}
