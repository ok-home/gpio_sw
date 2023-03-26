#include <stdio.h>
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <gpio_sw.h>

sw_gpio_cfg_t *cfg25,*cfg26,*cfg27 = NULL;

//  default event loop handle
//  gpio - port with key/switch
//  sw_data->sw_status - on/off level
//  sw_data->sw_cnt - key/switch pressed count
void sw_gpio_event_handle(void *args, esp_event_base_t base, int32_t id, void *data) 
{
    sw_gpio_out_t *sw_data = (sw_gpio_out_t *)data;
    ESP_LOGI("SW EVENT", "gpio = %lu lvl = %d cnt = %d", id, sw_data->sw_status, sw_data->sw_cnt);
}

void app_main(void)
{
    // example - 3 different mode key/switch 

    // switch with long on/off event on every switch
    cfg25 = sw_gpio_init(GPIO_NUM_25, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle);
    if(cfg25 == NULL) return; //error

    // switch with short on/off - switch faster then 0.5sek - one event after 0.5sek with counted switch
    cfg26 = sw_gpio_init(GPIO_NUM_26, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, DEFAULT_DETECT_TIME, sw_gpio_event_handle);
    if(cfg26 == NULL) return; //error

    // key short press/release  < 0.5sek - one event, >0.5sek multiply event
    cfg27 = sw_gpio_init(GPIO_NUM_27, SW_AUTO_GENERATE_MODE, DEFAULT_BOUNCEDELAY, DEFAULT_DETECT_TIME, sw_gpio_event_handle);
    if(cfg27 == NULL) return; //error
    
    for (;;)
    {
        vTaskDelay(100);
    }
    // on exit delete sw
    sw_gpio_delete(cfg25);
    sw_gpio_delete(cfg26);
    sw_gpio_delete(cfg27);
    // event loop delete after delete all sw
    sw_gpio_delete_event_loop();
}
