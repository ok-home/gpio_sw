//gpio_sw.h
#ifndef _GPIO_SW_H
#define _GPIO_SW_H

#define UNITY_TEST

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <driver/gpio.h>
#include <esp_event.h>

#define MIN_BOUNCEDELAY 10 / portTICK_PERIOD_MS
#define MAX_BOUNCEDELAY 100 / portTICK_PERIOD_MS
#define DEFAULT_BOUNCEDELAY 50 / portTICK_PERIOD_MS // время дребезга в тиках RTOS

#define MIN_DETECT_TIME 10 / portTICK_PERIOD_MS
#define MAX_DETECT_TIME 1000 / portTICK_PERIOD_MS
#define DEFAULT_DETECT_TIME 500 / portTICK_PERIOD_MS // время подсчета количества переключений, период автоинкремента в тиках FREERTOS

// переключастель с подавлением дребезга контактов
// sw_port - номер порта к которому подключен переключатель
// sw_status - состояние переключателя ( ON/OFF )
// sw_mode - режим переключателя
//  0 - SW_DEFAULT_MODE - обычный режим определяет измнение состояния переключателя
//  1 - SW_AUTO_GENERATE_MODE - формирует последовательность событий с заданной частотой при удержании кнопки ??
// sw_cnt - количество переключений состояния переключателя
//  0 - не было переключений
//  1-.... - количество переключений за время sw_detect_time
// sw_debounce_time - время подавления дребезга в тиках FREERTOS
// sw_detect_time - время подсчета количества переключений в тиках FREERTOS,
//                  при установке времени меньше sw_debounce_time - регистрация однократного переключения
//                  в режиме SW_AUTO_GENERATE_MODE время автоинкремента
// sw_debounce_task_hande - задача обработчик подавления дребезга и обработки состояния переключателя
// sw_event_handle  - event handle - вызов внешнего обработчика нажатия кнопок при регистрации переключений

typedef enum
{
    // режим для переключателей или однократного нажатия кнопок
    SW_DEFAULT_MODE = 0,
    // режим генерации нажатий при длительном нажатии кнопки
    SW_AUTO_GENERATE_MODE = 1,
} sw_gpio_mode_t;

typedef struct 
{
    uint16_t sw_status;         // out
    uint16_t sw_cnt;            // out
} sw_gpio_out_t;

typedef  void (*sw_event_handle_t)(void *args, esp_event_base_t base, int32_t id, void *data); // event handle callback 

struct sw_gpio_cfg;
typedef struct sw_gpio_cfg sw_gpio_cfg_t;

ESP_EVENT_DECLARE_BASE(SW_GPIO_EVENT_BASE);

sw_gpio_cfg_t* sw_gpio_init(gpio_num_t sw_port,sw_gpio_mode_t sw_mode,TickType_t sw_debounce_time,TickType_t sw_detect_time,void (*sw_event_handle)(void *args, esp_event_base_t base, int32_t id, void *data)); 
void sw_gpio_delete(sw_gpio_cfg_t *cfg);
void sw_gpio_delete_event_loop(void);
sw_gpio_out_t sw_gpio_read_status(sw_gpio_cfg_t *cfg);
sw_gpio_mode_t sw_gpio_set_mode (sw_gpio_cfg_t *cfg,sw_gpio_mode_t sw_mode);
TickType_t sw_gpio_set_debounce_time(sw_gpio_cfg_t *cfg,TickType_t sw_debounce_time);
TickType_t sw_gpio_set_detect_time(sw_gpio_cfg_t *cfg,TickType_t sw_detect_time);
// test only
#ifdef UNITY_TEST
TaskHandle_t sw_gpio_get_debounce_task_handle(sw_gpio_cfg_t *cfg);
#endif
#endif