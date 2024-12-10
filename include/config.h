/******************************************************************************
* FILENAME : config.h
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

// freertos
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

// esp
#include <hal/adc_types.h>

// propias
#include "system.h"

// Abstracciones para facilitar la legibilidad
#define CORE0 0
#define CORE1 1

// Configuraciones y constantes

// Nombre y estados de la máquina
#define SYS_NAME "STF P1 System"
enum{
	INIT,
    SENSOR_LOOP
};

// Configuración del termistor

#define THERMISTOR_ADC_UNIT ADC_UNIT_1
#define THERMISTOR_ADC_CHANNEL ADC_CHANNEL_6 // GPIO34
#define THERMISTOR_ADC_CHANNEL2 ADC_CHANNEL_7 // GPIO34

#define THERMISTOR_ADC_CHANNEL_ON_OFF GPIO_NUM_NC
#define THERMISTOR2_ADC_CHANNEL_ON_OFF 33

#define N 2

// Configuración del buffer cíclico
#define BUFFER_SIZE  2048
#define BUFFER_TYPE  RINGBUF_TYPE_NOSPLIT

// Configuración de las tareas

// SENSOR
// Tarea sensor
SYSTEM_TASK(TASK_SENSOR);
// definición de los argumentos que requiere la tarea
typedef struct 
{
	RingbufHandle_t* rbuf; // puntero al buffer 
	RingbufHandle_t* cbuf;
	uint8_t freq;          // frecuencia de muestreo
	uint8_t n; 
    // ...
}task_sensor_args_t;
// Timeout de la tarea (ver system_task_stop)
#define TASK_SENSOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_SENSOR_STACK_SIZE 4096

//Valores que se pasan al buffer
typedef struct {
    uint8_t source;  // 0 = TASK_SENSOR, 1 = TASK_CHECK
    float value;
} data_item_t;

// MONITOR
SYSTEM_TASK(TASK_MONITOR);
// definición de los argumentos que requiere la tarea
typedef struct 
{
	RingbufHandle_t* rbuf; // puntero al buffer 
    // ...
}task_monitor_args_t;
// Timeout de la tarea (ver system_task_stop)
#define TASK_MONITOR_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_MONITOR_STACK_SIZE 4096

// MONITOR
SYSTEM_TASK(TASK_CHECK);
// definición de los argumentos que requiere la tarea
typedef struct 
{
	RingbufHandle_t* rbuf;
	RingbufHandle_t* cbuf; // puntero al buffer 
    // ...
}task_check_args_t;
// Timeout de la tarea (ver system_task_stop)
#define TASK_CHECK_TIMEOUT_MS 2000 
// Tamaño de la pila de la tarea
#define TASK_CHECK_STACK_SIZE 4096

#endif