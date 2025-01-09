// libc
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>
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

static const char *TAG = "STF_P1:task_alerta";

// Tarea MONITOR
static float _diff_percent(uint16_t a, uint16_t b)
{
    if (a == 0 && b == 0) {
        return 0.0f; // Si ambos son cero, no hay diferencia
    }

    float fa = (float)a;
    float fb = (float)b;

    float diff = fabsf(fa - fb); // Diferencia absoluta
    float avg = (fa + fb) / 2.0f;

    if (avg < 1e-6) { // Evita dividir por un promedio muy pequeño
        return 0.0f;
    }

    return (diff / avg) * 100.0f; // Diferencia porcentual
}


SYSTEM_TASK(TASK_ALERTA)
{
    TASK_BEGIN();
    ESP_LOGI(TAG, "Task alerta running");

    // Recibe los argumentos de configuración de la tarea y los desempaqueta
    task_alerta_args_t* ptr_args = (task_alerta_args_t*) TASK_ARGS;
    RingbufHandle_t* abuf = ptr_args->abuf; 
    RingbufHandle_t* vbuf = ptr_args->vbuf; 
	uint16_t mascara = ptr_args->mascara;

    // Variables para reutilizar en el bucle
    size_t length;
    void *ptr = NULL;
    void *ptr2 = NULL;

    data_item_t* received_item[3];
    data_item_t res;

    // Inicializa valores por defecto
    memset(received_item, 0, sizeof(received_item));
    res.source = 100;
    
     // Loop
    TASK_LOOP()
    {
        // Se bloquea en espera de que haya algo que leer en el RingBuffer.
        // Tiene un timeout de 1 segundo para no bloquear indefinidamente la tarea,
        // pero si expira vuelve aquí sin consecuencias
        ptr = xRingbufferReceive(*abuf, &length, pdMS_TO_TICKS(1000));

        // Si recibimos exactamente 3 items, analizamos
        if (ptr != NULL && length == 3 * sizeof(data_item_t)) 
        {
            // Extraer los 3 items del bloque
            received_item[0] = (data_item_t*) ptr;
            received_item[1] = (data_item_t*) ptr + 1;
            received_item[2] = (data_item_t*) ptr + 2;

            // Obtener valores y sources
            float v0 = _therm_v2t(_therm_lsb2v(received_item[0]->value));
            float v1 = _therm_v2t(_therm_lsb2v(received_item[1]->value));
            float v2 = _therm_v2t(_therm_lsb2v(received_item[2]->value));

            uint8_t  s0 = received_item[0]->source;
            uint8_t  s1 = received_item[1]->source;
            uint8_t  s2 = received_item[2]->source;

            // Calcular diferencias 
            float d01 = _diff_percent(v0, v1);  // diferencia v0 vs. v1
            float d02 = _diff_percent(v0, v2);  // diferencia v0 vs. v2
            float d12 = _diff_percent(v1, v2);  // diferencia v1 vs. v2

           
            bool b01 = (d01 > mascara);  
            bool b02 = (d02 > mascara);
            bool b12 = (d12 > mascara);
            uint8_t pairs_out = (b01 ? 1 : 0) + (b02 ? 1 : 0) + (b12 ? 1 : 0);

            // Lógica de decisión
            //  1) Si 2 o más pares difieren > 10%, no podemos identificar un único sensor fallido => ERROR
            //  2) Si exactamente un sensor difiere de los otros dos (ambas comparaciones > 10%), => DEGRADATED_MODE
            //  3) Si no hay diferencia o no se cumple lo anterior, no hacemos nada especial (o según se requiera)

            if (pairs_out > 2) 
            {
                // Hay al menos 2 pares con discrepancia > 10% => no se puede aislar a un único sensor
               
                SWITCH_ST_FROM_TASK(ERROR);
            }
            else 
            {
                // Revisamos si existe exactamente un sensor alejado de los otros dos
                // Para eso contamos cuántas “quejas” (out-of-range) tiene cada sensor:
                // b01 => conflicto entre s0 y s1
                // b02 => conflicto entre s0 y s2
                // b12 => conflicto entre s1 y s2
                uint8_t out0 = 0;
                uint8_t out1 = 0;
                uint8_t out2 = 0;

                if (b01) { out0++; out1++; }
                if (b02) { out0++; out2++; }
                if (b12) { out1++; out2++; }

                // Un sensor “falla” si tiene outX == 2 (es decir, difiere con ambos)
                // Si uno solo lo cumple => DEGRADATED_MODE; si lo cumplen varios => ERROR
                uint8_t count_fail = 0;
                if (out0 == 2) count_fail++;
                if (out1 == 2) count_fail++;
                if (out2 == 2) count_fail++;

                if (count_fail == 1) 
                {
                    
                    // Identificamos quién es el que falla
                    if (out0 == 2) {
                        res.value = s0;
                        
                    }
                    else if (out1 == 2) {
                        res.value = s1;
                        
                    }
                    else if (out2 == 2) {
                       res.value= s2;
                        
                    }

                    if(GET_ST_FROM_TASK() != DEGRADED_MODE){
                        SWITCH_ST_FROM_TASK(DEGRADED_MODE);
                    }

                    if (xRingbufferSendAcquire(*vbuf, &ptr2, sizeof(data_item_t), pdMS_TO_TICKS(100)) != pdTRUE)
				    {
                        // Si falla la reserva de memoria, notifica la pérdida del dato. Esto ocurre cuando 
                        // una tarea productora es mucho más rápida que la tarea consumidora. Aquí no debe ocurrir.
                        ESP_LOGI(TAG,"Buffer lleno. Espacio disponible: %d", xRingbufferGetCurFreeSize(*vbuf));
                    }
                    else 
                    {
                        // Si xRingbufferSendAcquire tiene éxito, podemos escribir el número de bytes solicitados
                        // en el puntero ptr. El espacio asignado estará bloqueado para su lectura hasta que 
                        // se notifique que se ha completado la escritura
                        memcpy(ptr2,&res,sizeof(data_item_t));

                        // Se notifica que la escritura ha completado. 
                        xRingbufferSendComplete(*vbuf, ptr2);
				    }

                }
                else if (count_fail > 1) 
                {
                    // Más de un sensor difiere de los otros => no se puede determinar uno solo
                    
                    SWITCH_ST_FROM_TASK(ERROR);
                }
                else 
                {
                    //No difiere ninguno
                    if(GET_ST_FROM_TASK() != NORMAL_MODE){
                        SWITCH_ST_FROM_TASK(NORMAL_MODE);
                    }
                    
                }
            }

            if (ptr != NULL) {
                vRingbufferReturnItem(*abuf, ptr);
            }
        } 
        else 
        {
            // Si llegó algo pero no son 3 ítems, lo retornamos también
            if (ptr != NULL) {
                vRingbufferReturnItem(*vbuf, ptr);
            }
            ESP_LOGW(TAG, "Esperando datos ...");
        }

    } // Fin del TASK_LOOP

    ESP_LOGI(TAG,"Deteniendo la tarea ...");
    TASK_END();
}