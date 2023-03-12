#include "unity.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "gpio_sw.h"

typedef struct gpioEmul
{
    uint32_t port_def;
    gpio_mode_t mode;
    uint32_t lvl;
    uint32_t irq_def;
    gpio_isr_t irq_handler;
} gpioEmul_t;

gpioEmul_t ff_gpio_table[40] = {0};

// ff gpio function
esp_err_t ff_gpio_config(const gpio_config_t *);
int ff_gpio_get_level(gpio_num_t);
esp_err_t ff_gpio_set_level(gpio_num_t, int);
esp_err_t ff_gpio_isr_handler_add(gpio_num_t port, gpio_isr_t irq_handler, void *args);
gpio_isr_t ff_gpio_isr_handler_get(gpio_num_t port);

// redefined  gpio function

esp_err_t __wrap_gpio_config(const gpio_config_t *cfg)
{
    return (ff_gpio_config(cfg));
}
int __wrap_gpio_get_level(gpio_num_t port)
{
    return (ff_gpio_get_level(port));
}
esp_err_t __wrap_gpio_set_level(gpio_num_t port, uint32_t lvl)
{
    return (ff_gpio_set_level(port, lvl));
}
esp_err_t __wrap_gpio_isr_handler_add(gpio_num_t port, gpio_isr_t irq_handler, void *args)
{
    return (ff_gpio_isr_handler_add(port, irq_handler, args));
}

// ff gpio function
esp_err_t ff_gpio_config(const gpio_config_t *cfg)
{
    gpio_config_t *cnf = (gpio_config_t *)cfg;
    for (int i = 0; i < 40; i++)
    {
        if (cnf->pin_bit_mask & 1)
        {
            ff_gpio_table[i].port_def = 1;
            ff_gpio_table[i].mode = cnf->mode;
            ff_gpio_table[i].lvl = 0;
            ff_gpio_table[i].irq_handler = NULL;
        }
        cnf->pin_bit_mask = cnf->pin_bit_mask >> 1;
    }
    return ESP_OK;
}
int ff_gpio_get_level(gpio_num_t port)
{
    if (port > 40 || ff_gpio_table[port].port_def == 0)
        return ESP_FAIL;
    return (ff_gpio_table[port].lvl);
}
esp_err_t ff_gpio_set_level(gpio_num_t port, int lvl)
{
    if (port > 40 || ff_gpio_table[port].port_def == 0)
        return ESP_FAIL;
    ff_gpio_table[port].lvl = lvl;
    return ESP_OK;
}
esp_err_t ff_gpio_isr_handler_add(gpio_num_t port, gpio_isr_t irq_handler, void *args)
{
    if (port > 40 || ff_gpio_table[port].port_def == 0)
        return ESP_FAIL;
    ff_gpio_table[port].irq_def = 1;
    ff_gpio_table[port].irq_handler = irq_handler;
    return ESP_OK;
}
gpio_isr_t ff_gpio_isr_handler_get(gpio_num_t port)
{
    if (port > 40 || ff_gpio_table[port].port_def == 0 || ff_gpio_table[port].irq_def == 0)
        return NULL;
    return (ff_gpio_table[port].irq_handler);
}

QueueHandle_t xQueue_tst;  //  gpio 2
QueueHandle_t xQueue_tst1; // gpio 6

void sw_gpio_event_handle(void *args, esp_event_base_t base, int32_t id, void *data) // gpio2
{
    sw_gpio_out_t *sw_data = (sw_gpio_out_t *)data;
//    ESP_LOGI("SW EVENT", "gpio = %lu lvl = %d cnt = %d", id, sw_data->sw_status, sw_data->sw_cnt);
    xQueueSend(xQueue_tst, sw_data, portMAX_DELAY);
}
void sw_gpio_event_handle1(void *args, esp_event_base_t base, int32_t id, void *data) // gpio6
{
    sw_gpio_out_t *sw_data = (sw_gpio_out_t *)data;
//    ESP_LOGI("SW EVENT", "gpio = %lu lvl = %d cnt = %d", id, sw_data->sw_status, sw_data->sw_cnt);
    xQueueSend(xQueue_tst1, sw_data, portMAX_DELAY);
}

void sw_gpio_event_handle_26(void *args, esp_event_base_t base, int32_t id, void *data) // gpio2 & gpio6
{
    sw_gpio_out_t *sw_data = (sw_gpio_out_t *)data;
//    ESP_LOGI("SW EVENT", "gpio = %lu lvl = %d cnt = %d", id, sw_data->sw_status, sw_data->sw_cnt);
    switch (id)
    {
    case GPIO_NUM_2:
        xQueueSend(xQueue_tst, sw_data, portMAX_DELAY);
        break;
    case GPIO_NUM_6:
        xQueueSend(xQueue_tst1, sw_data, portMAX_DELAY);
        break;
    default:
        ESP_LOGI("SW EVENT", "ERROR gpio = %lu", id);
    }
}

TEST_CASE("gpio_sw_init", "[GPIO SW]")
{
    sw_gpio_cfg_t *cfg = sw_gpio_init(GPIO_NUM_6, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, DEFAULT_DETECT_TIME, sw_gpio_event_handle);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(sw_gpio_get_debounce_task_handle(cfg), 0, "task create - task handle not null");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ff_gpio_isr_handler_get(GPIO_NUM_6), NULL, "ISR install & add - ISR handle not null");
    gpio_sw_delete(cfg);
    gpio_sw_delete_event_loop();
    // проверка на номер порта
    cfg = sw_gpio_init(60, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, DEFAULT_DETECT_TIME, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(cfg, NULL, "invalid port number");
    gpio_sw_delete(cfg);
    gpio_sw_delete_event_loop();
}

TEST_CASE("TaskNotify handle short press short only", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, NULL);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 2); // 70ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 2); // 70ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    gpio_sw_delete(cfg);
}

TEST_CASE("IRQ handle short/long press", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_3;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, NULL);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    gpio_isr_t isr_handle = ff_gpio_isr_handler_get(gpio_port);
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 2); // 70ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 2); // 70ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off not detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 0, "on/off not detect 1 cnt 0 -70ms");

    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 10); // 70ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    sw_gpio_set_detect_time(cfg, DEFAULT_DETECT_TIME);

    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_DETECT_TIME + DEFAULT_BOUNCEDELAY + 5); // 500ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 500ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 500ms");

    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_DETECT_TIME + DEFAULT_BOUNCEDELAY + 5); // 500ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 1 500ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 500ms");

    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY + 2); // 50ms - default debounce 50ms
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    vTaskDelay(DEFAULT_DETECT_TIME + DEFAULT_BOUNCEDELAY + 5); // 500ms - default debounce 50ms
    result = sw_gpio_read_status(cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 500ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 2, "on/off detect 2 cnt 1 500ms");

    gpio_sw_delete(cfg);
}

TEST_CASE("TaskNotify Event loop handle short press short only", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    xQueue_tst = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);

    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);

    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    vQueueDelete(xQueue_tst);
    gpio_sw_delete(cfg);
    gpio_sw_delete_event_loop();
}

TEST_CASE("TaskNotify Event loop handle multiply handle", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    xQueue_tst = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_num_t gpio_port1 = GPIO_NUM_6;
    sw_gpio_cfg_t *cfg1 = sw_gpio_init(gpio_port1, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle1);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg1, NULL, "null ptr error");
    xQueue_tst1 = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle1 = sw_gpio_get_debounce_task_handle(cfg1);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    // next port

    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    vQueueDelete(xQueue_tst);
    vQueueDelete(xQueue_tst1);

    gpio_sw_delete(cfg);
    gpio_sw_delete(cfg1);
    gpio_sw_delete_event_loop();
}

TEST_CASE("TaskNotify Event loop handle multiply gpio one handle", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle_26);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    xQueue_tst = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_num_t gpio_port1 = GPIO_NUM_6;
    sw_gpio_cfg_t *cfg1 = sw_gpio_init(gpio_port1, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle_26);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg1, NULL, "null ptr error");
    xQueue_tst1 = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle1 = sw_gpio_get_debounce_task_handle(cfg1);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    // next port

    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 1);
    xTaskNotifyGive(task_handle1);
    gpio_set_level(gpio_port1, 0);
    xTaskNotifyGive(task_handle1);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    vQueueDelete(xQueue_tst);
    vQueueDelete(xQueue_tst1);

    gpio_sw_delete(cfg);
    gpio_sw_delete(cfg1);
    gpio_sw_delete_event_loop();
}

TEST_CASE("IRQ Event loop handle multiply gpio one handle", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle_26);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    xQueue_tst = xQueueCreate(10, sizeof(sw_gpio_out_t));
    gpio_isr_t isr_handle = ff_gpio_isr_handler_get(gpio_port);
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_num_t gpio_port1 = GPIO_NUM_6;
    sw_gpio_cfg_t *cfg1 = sw_gpio_init(gpio_port1, SW_DEFAULT_MODE, DEFAULT_BOUNCEDELAY, MIN_DETECT_TIME, sw_gpio_event_handle_26);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg1, NULL, "null ptr error");
    xQueue_tst1 = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle1 = sw_gpio_get_debounce_task_handle(cfg1);

    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 1);
    isr_handle(task_handle);
    gpio_set_level(gpio_port, 0);
    isr_handle(task_handle1);
    xQueueReceive(xQueue_tst, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    // next port

    gpio_set_level(gpio_port1, 1);
    isr_handle(task_handle1);
    gpio_set_level(gpio_port1, 0);
    isr_handle(task_handle1);
    gpio_set_level(gpio_port1, 1);
    isr_handle(task_handle1);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "on/off detect 1 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 1 cnt 1 -70ms");

    gpio_set_level(gpio_port1, 0);
    isr_handle(task_handle1);
    gpio_set_level(gpio_port1, 1);
    isr_handle(task_handle1);
    gpio_set_level(gpio_port1, 0);
    xQueueReceive(xQueue_tst1, &result, DEFAULT_BOUNCEDELAY + 20);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 0, "on/off detect 0 -70ms");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "on/off detect 0 cnt 1 -70ms");

    vQueueDelete(xQueue_tst);
    vQueueDelete(xQueue_tst1);

    gpio_sw_delete(cfg);
    gpio_sw_delete(cfg1);
    gpio_sw_delete_event_loop();
}

TEST_CASE("TaskNotify auto generate mode", "[GPIO SW]")
{
    sw_gpio_out_t result;
    gpio_num_t gpio_port = GPIO_NUM_2;
    sw_gpio_cfg_t *cfg = sw_gpio_init(gpio_port, SW_AUTO_GENERATE_MODE, DEFAULT_BOUNCEDELAY, DEFAULT_DETECT_TIME, sw_gpio_event_handle);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(cfg, NULL, "null ptr error");
    xQueue_tst = xQueueCreate(10, sizeof(sw_gpio_out_t));
    TaskHandle_t task_handle = sw_gpio_get_debounce_task_handle(cfg);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);

    vTaskDelay(DEFAULT_DETECT_TIME * 4);
    sw_gpio_set_detect_time(cfg, DEFAULT_DETECT_TIME / 2);
    vTaskDelay(DEFAULT_DETECT_TIME * 2);

    for (int i = 1; i < 9; i++)
    {
        xQueueReceive(xQueue_tst, &result, DEFAULT_DETECT_TIME * 2);
        TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "key press and hold");
        TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, i, "1-8 times repeat");
    }
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);

    vTaskDelay(DEFAULT_BOUNCEDELAY * 2);

    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    vTaskDelay(DEFAULT_BOUNCEDELAY * 2);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 1);
    xTaskNotifyGive(task_handle);
    gpio_set_level(gpio_port, 0);
    xTaskNotifyGive(task_handle);

    xQueueReceive(xQueue_tst, &result, DEFAULT_DETECT_TIME * 2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_status, 1, "key press and release");
    TEST_ASSERT_EQUAL_INT_MESSAGE(result.sw_cnt, 1, "1 times repeat");

    vQueueDelete(xQueue_tst);
    gpio_sw_delete(cfg);
    gpio_sw_delete_event_loop();
}