#define UNITY_TEST
#include <gpio_sw.h>
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

struct sw_gpio_cfg
{
    gpio_num_t sw_port;                   // in
    sw_gpio_mode_t sw_mode;               // in
    sw_gpio_out_t sw_out;                 // out
    TickType_t sw_debounce_time;          // in
    TickType_t sw_detect_time;            // in
    TaskHandle_t sw_debounce_task_handle; // internal out
    sw_event_handle_t sw_event_handle;    // event handle callback
    portMUX_TYPE sw_spinlock;
};

static esp_event_loop_handle_t sw_gpio_event_task = NULL;

ESP_EVENT_DEFINE_BASE(SW_GPIO_EVENT_BASE);

static void sw_local_callback(char *tag, sw_gpio_cfg_t *gpio_sw)
{
    //    ESP_LOGI(tag, "SW lvl %d cnt %d period %lu", gpio_sw->sw_out.sw_status, gpio_sw->sw_out.sw_cnt, gpio_sw->sw_detect_time);
    // cb call;
    if (sw_gpio_event_task)
    {
        esp_event_post_to(sw_gpio_event_task, SW_GPIO_EVENT_BASE, gpio_sw->sw_port, &(gpio_sw->sw_out), sizeof(sw_gpio_out_t), portMAX_DELAY);
    }
}

static void IRAM_ATTR sw_isr_handler(void *handle)
{
    BaseType_t xHigherPriorityTaskWoken = 0;
    vTaskNotifyGiveFromISR((TaskHandle_t)handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
}

static void sw_debounce_task(void *cfg)
{
    sw_gpio_cfg_t *gpio_sw = (sw_gpio_cfg_t *)cfg;
    TickType_t delay = portMAX_DELAY;
    uint32_t cntisr = 0;
    sw_gpio_out_t lc_sw_out; // прерываний в очереди
    lc_sw_out.sw_status = gpio_get_level(gpio_sw->sw_port);
    lc_sw_out.sw_cnt = 0;
    sw_gpio_set_status(gpio_sw, lc_sw_out); // текущее состояние порта
    int cur_lvl = lc_sw_out.sw_status;      // ( предыдущее значение )зафиксированное состояние переключателя
    int sw_lvl = cur_lvl;                   // текущее состояние
    TickType_t expired_time = 0;            // xTaskGetTickCount(); // количество циклов до срабатывания //TickType_t xTaskGetTickCount( void );
    int cnt_press = lc_sw_out.sw_cnt = 0;

    for (;;)
    {
        cntisr = ulTaskNotifyTake(pdFALSE, delay); // есть некоторое количество прерываний
        if (cntisr != 0)                           // interupt from delay time
        {
            if (delay == portMAX_DELAY)
            {
                expired_time = xTaskGetTickCount() + sw_gpio_get_detect_time(gpio_sw); // gpio_sw->sw_detect_time;
            }
            delay = sw_gpio_get_debounce_time(gpio_sw); // gpio_sw->sw_debounce_time; // проверяем дребезг за время задержки
            continue;                                   // пока отрабатываем каждый фронт на дребезге; cntisr - должен уменьшаться на каждом вызове ulTaskNotifyTake
        }
        // за время задержки дребезга не было прерываний - дребезг завершен фиксируем
        cur_lvl = gpio_get_level(gpio_sw->sw_port);
        if (sw_gpio_get_mode(gpio_sw) == SW_DEFAULT_MODE)
        {
            // зафиксировано переключение после дребезга
            if (sw_lvl != cur_lvl) // было переключение, не только дребезг
            {
                cnt_press++;
            }
            sw_lvl = cur_lvl;
            lc_sw_out.sw_status = cur_lvl;
            lc_sw_out.sw_cnt = cnt_press;
            sw_gpio_set_status(gpio_sw, lc_sw_out);
            if (expired_time >= xTaskGetTickCount())
            {
                continue;
            }
            if (cnt_press)
            {
                sw_local_callback("GPIO_SW", gpio_sw);
            }
            cnt_press = 0;
            delay = portMAX_DELAY; // в следующем цикле - ожидаем новый пакет прерываний
        }
        else //
        {
            if (sw_lvl != cur_lvl) // было переключение, не только дребезг
            {
                sw_lvl = cur_lvl;
                lc_sw_out.sw_status = cur_lvl;
                sw_gpio_set_status(gpio_sw, lc_sw_out);
                if (cnt_press == 0) // первое переключение
                {
                    cnt_press++;
                    delay = sw_gpio_get_detect_time(gpio_sw); // gpio_sw->sw_detect_time;
                    lc_sw_out.sw_cnt = cnt_press;
                    sw_gpio_set_status(gpio_sw, lc_sw_out);
                    sw_local_callback("GPIO_SW_CONTINUE", gpio_sw);
                }
                else // обратное переключение
                {
                    cnt_press = 0;
                    lc_sw_out.sw_cnt = cnt_press;
                    sw_gpio_set_status(gpio_sw, lc_sw_out);
                    delay = portMAX_DELAY;
                }
            }
            else
            {
                if (cnt_press)
                {
                    cnt_press++;
                    lc_sw_out.sw_cnt = cnt_press;
                    sw_gpio_set_status(gpio_sw, lc_sw_out);
                    delay = sw_gpio_get_detect_time(gpio_sw); // gpio_sw->sw_detect_time;
                    sw_local_callback("GPIO_SW_CONTINUE", gpio_sw);
                }
            }
        }
    }
}

sw_gpio_cfg_t *sw_gpio_init(gpio_num_t sw_port, sw_gpio_mode_t sw_mode, TickType_t sw_debounce_time, TickType_t sw_detect_time, void (*sw_event_handle)(void *args, esp_event_base_t base, int32_t id, void *data))
{
    esp_err_t ret;
    sw_gpio_cfg_t *cfg = calloc(1, sizeof(sw_gpio_cfg_t));
    if (cfg == NULL)
    {
        return cfg;
    }
    cfg->sw_spinlock.owner = portMUX_FREE_VAL;
    cfg->sw_spinlock.count = 0;

    sw_gpio_set_mode(cfg, sw_mode);
    sw_gpio_set_debounce_time(cfg, sw_debounce_time);
    sw_gpio_set_detect_time(cfg, sw_detect_time);


    cfg->sw_port = sw_port;
    // gpio pin config
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .pin_bit_mask = 1ULL << cfg->sw_port,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};
    if (gpio_config(&io_conf))
    {
        goto _err_alloc;
    }

    esp_event_loop_args_t gpio_sw_event_args = {
        .queue_size = 5,
        .task_name = "gpio_sw_event_task", // task will be created
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 2048 + 1024, // 2048,
        .task_core_id = tskNO_AFFINITY};

    if (sw_event_handle) // в параметрах задан обработчик событий - создаем event loop
    {
        if (sw_gpio_event_task == NULL) // event loop создается только 1 раз, если не создано - создаем
        {
            ret = esp_event_loop_create(&gpio_sw_event_args, &sw_gpio_event_task);
            if (ret)
                goto _err_alloc;
        }
        cfg->sw_event_handle = sw_event_handle;
        ret = esp_event_handler_instance_register_with(sw_gpio_event_task, SW_GPIO_EVENT_BASE, cfg->sw_port, cfg->sw_event_handle, NULL, NULL);
        if (ret)
            goto _err_alloc;
    }

    cfg->sw_out.sw_status = gpio_get_level(cfg->sw_port);
    cfg->sw_out.sw_cnt = 0;

    ret = xTaskCreate(sw_debounce_task, "sw_debounce", 2048, cfg, uxTaskPriorityGet(NULL), &cfg->sw_debounce_task_handle);
    if (ret != pdPASS)
    {
        goto _err_alloc;
    }

    // install gpio isr service // возможно уже установлен
    ret = gpio_install_isr_service(0); // может выйти с ошибкой если уже установлен
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        goto _del_task;
    // hook isr handler for specific gpio pin
    ret = gpio_isr_handler_add(cfg->sw_port, sw_isr_handler, (void *)cfg->sw_debounce_task_handle);
    if (ret != ESP_OK)
        goto _del_task;

    return cfg;

_del_task:
    vTaskDelete(cfg->sw_debounce_task_handle);

_err_alloc:
    free(cfg);
    return NULL;
}

void sw_gpio_set_mode(sw_gpio_cfg_t *cfg, sw_gpio_mode_t sw_mode)
{
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    if (sw_mode > SW_AUTO_GENERATE_MODE)
    {
        cfg->sw_mode = SW_DEFAULT_MODE;
    }
    else
    {
        cfg->sw_mode = sw_mode;
    }
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
}
sw_gpio_mode_t sw_gpio_get_mode(sw_gpio_cfg_t *cfg)
{
    sw_gpio_mode_t ret;
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    ret = cfg->sw_mode;
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
    return ret;
}

void sw_gpio_set_debounce_time(sw_gpio_cfg_t *cfg, TickType_t sw_debounce_time)
{
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    if (sw_debounce_time < MIN_BOUNCEDELAY || sw_debounce_time > MAX_BOUNCEDELAY)
    {
        cfg->sw_debounce_time = DEFAULT_BOUNCEDELAY;
    }
    else
    {
        cfg->sw_debounce_time = sw_debounce_time;
    }
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
}
TickType_t sw_gpio_get_debounce_time(sw_gpio_cfg_t *cfg)
{
    TickType_t ret;
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    ret = cfg->sw_debounce_time;
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
    return ret;
}
void sw_gpio_set_detect_time(sw_gpio_cfg_t *cfg, TickType_t sw_detect_time)
{
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    if (sw_detect_time < MIN_DETECT_TIME || sw_detect_time > MAX_DETECT_TIME)
    {
        cfg->sw_detect_time = DEFAULT_DETECT_TIME;
    }
    else
    {
        cfg->sw_detect_time = sw_detect_time;
    }
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
}
TickType_t sw_gpio_get_detect_time(sw_gpio_cfg_t *cfg)
{
    TickType_t ret;
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    ret = cfg->sw_detect_time;
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
    return ret;
}
sw_gpio_out_t sw_gpio_get_status(sw_gpio_cfg_t *cfg)
{
    sw_gpio_out_t ret;
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    ret = cfg->sw_out;
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
    return ret;
}
void sw_gpio_set_status(sw_gpio_cfg_t *cfg, sw_gpio_out_t value)
{
    taskENTER_CRITICAL(&(cfg->sw_spinlock));
    cfg->sw_out = value;
    taskEXIT_CRITICAL(&(cfg->sw_spinlock));
}
void sw_gpio_delete(sw_gpio_cfg_t *cfg)
{
    if (cfg)
    {
        gpio_isr_handler_remove(cfg->sw_port);
        vTaskDelete(cfg->sw_debounce_task_handle);
        cfg->sw_debounce_task_handle = NULL;
        esp_event_handler_unregister(SW_GPIO_EVENT_BASE, cfg->sw_port, cfg->sw_event_handle);
        free(cfg);
    }
}
void sw_gpio_delete_event_loop()
{
    if (sw_gpio_event_task)
    {
        esp_event_loop_delete(sw_gpio_event_task);
        sw_gpio_event_task = NULL;
    }
}

// test only
#ifdef UNITY_TEST
TaskHandle_t sw_gpio_get_debounce_task_handle(sw_gpio_cfg_t *cfg)
{
    return cfg->sw_debounce_task_handle;
}
#endif