// https://esp32tutorials.com/esp32-uart-tutorial-esp-idf/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

/**
 * Это пример, который возвращает любые данные, которые он получает через настроенный UART, обратно отправителю,
 * с отключенным аппаратным управлением потоком. Он не использует очередь событий драйвера UART.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (gpio_num_t::GPIO_NUM_4) // GPIO number for UART TX
#define ECHO_TEST_RXD (gpio_num_t::GPIO_NUM_5) // GPIO number for UART RX
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (0)      // UART port number
#define ECHO_UART_BAUD_RATE     (115200) // Скорость связи UART для примера Modbus
#define ECHO_TASK_STACK_SIZE    (2048)   // stack size for UART

#define BUF_SIZE (1024)

static void echo_task(void *arg) {

    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
    /*
    Функции которые устанавливают конкретные свойства:
        uart_set_baudrate()     - Скорость передачи данных	
        uart_set_word_length()  - Количество переданных битов	, выбран из uart_word_length_t
        uart_set_parity()       - Контроль четности, выбран из uart_parity_t
        uart_set_stop_bits()    - Количество стоповых битов, выбран из uart_stop_bits_t
        uart_set_hw_flow_ctrl() - Аппаратный режим управления потоком, выбран из uart_hw_flowcontrol_t
        uart_set_mode()         - Режим связи, выбран из uart_mode_t
    */

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
    
    /*
    Функция uart_driver_install выделяет ресурсы ESP32 для драйвера UART

    esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags)
        uart_num - номер порта UART
        rx_buffer_size - Размер кольцевого буфера RX
        tx_buffer_size - Размер кольцевого буфера TX
        queue_size - Размер очереди событий UART
        uart_queue - Дескриптор очереди событий UART
        intr_alloc_flags - Флаги для выделения прерывания
    */
    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    
    /*
        Функция uart_set_pin() вызывается для установки контактов TX, RX, RTS и CTS, которые мы передаем в качестве параметров внутри нее. 
        esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num)

        uart_num — это номер порта UART. 
        tx_io_num — это вывод GPIO, который мы хотим настроить как вывод TX. 
        rx_io_num — это вывод GPIO, который мы хотим настроить как вывод RX. 
        rts_io_num — это вывод GPIO, который мы хотим настроить как вывод RTS. 
        cts_io_num — это вывод GPIO, который мы хотим настроить как вывод CTS. 
    */
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    // FSM берет на себя весь процесс, посредством которого приложение просто читает и записывает данные в определенный буфер.
    while (1) {
        /*
        Чтение данных из UART
        Для последовательного получения данных FSM сначала обрабатывает входящие данные и распараллеливает их
        Затем он записывает данные в буфер RX FIFO. Наконец, эти данные считываются из буфера RX FIFO

        int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait)
            uart_num - это номер порта UART
            buf - указатель на буфер
            length - длина данных
            ticks_to_wait - количество тактов RTOS
        */
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        
        /*
        Запись данных в UART
        Чтобы отправить данные последовательно, мы сначала записываем данные в буфер TX FIFO
        Затем FSM сериализует эти данные и отправляет их.

        int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size)
            uart_num - это номер порта UART
            src - адрес буфера данных
            size - длина отправляемых данных
        */
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);
    }
}

void run_uart(void)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}