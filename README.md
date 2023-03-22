#  sw_gpio - ESP-IDF GPIO based 
## Библиотека обслуживания переключателей/кнопок
- Произвольное количество кнопок ( экземпляр обработчика на каждый GPIO )
- Нажатия/переключения регистрируются по прерываниям GPIO, не затрачивается время на периодический опрос кнопок
- Обработчик работает в 2-х режимах
1. Режим переключателя - детектирует переключения вкл/выкл и быстрые переключения вкл/выкл/вкл/выкл
2. Режим кнопки  - простое нажатие/отпускание кнопки, при продолжительном нажатии генерируется периодическое нажатие кнопки.
- При обнаружении нажатия/отпускания кнопки вкл/выкл переключателя генрируется event ESP-IDF event loop
1. Номер порта GPIO подключенного к кнопке/переключателю
2. Состояние кнопки/переключателя
3. Количество срабатываний кнопки/переключателя в интервале времени

## Использование библиотеки
    sw_gpio_cfg_t *cfg; // указатель на экземпляр обработчика - отдельно для каждой кнопки
    //
    // Создать экземпляр обработчика, начать обработку
    // gpio_num_t sw_port - номер порта GPIO
    // sw_gpio_mode_t sw_mode - режим работы обработчика
    //      SW_DEFAULT_MODE - режим переключателя
    //      SW_AUTO_GENERATE_MODE - режим кнопки
    // TickType_t sw_debounce_time - время обработки дребезга в тиках FreeRTOS 
    // TickType_t sw_detect_time
    //      в режиме переключателя определяет время в течении которого регистрируются быстрые переключения в тиках FreeRTOS
    //   например в течении 0,5 секунды переключатель переключили несколько раз
    //      в режиме кнопки определяет интервал автонажатия кнопки в тиках FreeRTOS
    // void (*sw_event_handle)(void *args, esp_event_base_t base, int32_t id, void *data) - обработчик event loop ESP-IDF
    //
    sw_gpio_cfg_t* sw_gpio_init(gpio_num_t sw_port,sw_gpio_mode_t sw_mode,TickType_t sw_debounce_time,TickType_t sw_detect_time,void (*sw_event_handle)(void *args, esp_event_base_t base, int32_t id, void *data)); 
    //
    // обработчик event loop ESP-IDF
    // может определятся как для каждого GPIO отдельно, так и один обработчик для всех или группы GPIO
    // id - номер GPIO
    // data - ссылка на структуру
        typedef struct {
        uint16_t sw_status;         // состояние переключателя/кнопки 0/1
        uint16_t sw_cnt;            // количество переключений в интервале или автонажатий кнопки в интервале
        } sw_gpio_out_t;
    //
    void sw_gpio_event_handle(void *args, esp_event_base_t base, int32_t id, void *data)
    //
    void sw_gpio_delete(sw_gpio_cfg_t *cfg);// удалить обработчик
    void sw_gpio_delete_event_loop(void); // удалить event loop после удаления всех обработчиков
    sw_gpio_out_t sw_gpio_read_status(sw_gpio_cfg_t *cfg);// прочитать состояние кнопки
    sw_gpio_mode_t sw_gpio_set_mode (sw_gpio_cfg_t *cfg,sw_gpio_mode_t sw_mode);//изменить режим работы обработчика
    TickType_t sw_gpio_set_debounce_time(sw_gpio_cfg_t *cfg,TickType_t sw_debounce_time);//изменить время обработки дребезга
    TickType_t sw_gpio_set_detect_time(sw_gpio_cfg_t *cfg,TickType_t sw_detect_time);//изменить время регистрации быстрых переключений/время автоповтора

Пример использования - в папке example





