#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "FreeRTOS.h"
#include "task.h"
#include "math.h"
#include "semphr.h"
#include <stdio.h>
#include "pico/stdlib.h"

#define SPI_PORT spi0
#define CSN 17
#define SCK 18
#define MOSI 19
#define DC 12
#define RESET 13

SemaphoreHandle_t spi_mtx;
SemaphoreHandle_t dma_sema;
TaskHandle_t spi_handle;
BaseType_t xHigherPriorityTaskWoken;
uint8_t frame_buf[2*128*160];
uint8_t zero_buf[256] = {0};
int dma_tx_chan;
int j = 0;
int l = 0;

void send_command(uint8_t cmd);
void send_data(uint8_t* data, size_t length);
void write_frame_buffer(uint8_t* frame_buffer);
void set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void dma_init();
void gpio_setup();

void spi_task(void *pvParams);
void frame_buf_task(void *pvParams);

void __isr dma_callback(){
    dma_hw->ints0 = 1 << dma_tx_chan;
    while (spi_is_busy(SPI_PORT));
    gpio_put(CSN, 1);
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(dma_sema, &xHigherPriorityTaskWoken);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int main() {

    stdio_init_all();
    gpio_setup();
    dma_init();

    spi_mtx = xSemaphoreCreateMutex();

    if (spi_mtx != NULL) {
        xTaskCreate(spi_task, "SPI_Task", 1024, NULL, 1, &spi_handle);
        xTaskCreate(frame_buf_task, "Frame_task", 256, NULL, 1, NULL);
        vTaskStartScheduler();
    }

    while (true);
}


void spi_task(void *pvParams) {
    uint8_t buf = 0x05;
    spi_init(SPI_PORT, 20*1000 * 1000);
    spi_set_format (SPI_PORT, 8, 0, 1, SPI_MSB_FIRST);
    send_command(0x11);
    send_command(0x3a);
    send_data(&buf, 1);
    
    for(uint8_t i = 0;i< 160;i++){
        send_data(zero_buf, sizeof(zero_buf));
    }
    send_command(0x29);
    while (true) {
        write_frame_buffer(frame_buf);
        // vTaskDelay(pdMS_TO_TICKS(16));
        taskYIELD();
    }
}

void frame_buf_task(void *pvParams){
    uint8_t ind = 0;
    uint8_t offset = 0;
    float phase = 0.0;
    // uint8_t intensity;
    while(true){
        // for(int k = 0; k < 160; k++) {
        //     for(int i = 0; i < 128; i++) {
        //         // Generate a "noisy" pixel
        //         // rand() % 255 gives random grayscale
        //         intensity = (uint8_t)(rand() % 256);
                
        //         int index = (k * 128 + i) * 2;
                
        //         // Writing the same intensity to both bytes 
        //         // creates a shade of gray in RGB565
        //         frame_buf[index] = intensity;
        //         frame_buf[index + 1] = intensity;
        //     }
        // }
        // for(int i = 0; i < (128 * 160 * 2); i++) {
        //     // Mix a random number with a shifting offset
        //     // This creates "moving" noise
        //     frame_buf[i] = (uint8_t)(rand() % 128) + offset;
        // }
        // offset++;


        // 2. Loop through every column (x-axis)
        memset(frame_buf, 0, sizeof(frame_buf));

        // 2. Loop through every row (y-axis)
        for(int y = 0; y < 160; y++) {
            
            // Calculate x: center_x + (amplitude * sin(freq * y + phase))
            // Center is 64 (middle of 128), Amplitude is 30 pixels
            int x = 64 + (int)(40.0 * sinf((y * 0.05) + phase));

            // Safety check: ensure x is within 0-127
            if(x >= 0 && x < 128) {
                int index = (y * 128 + x) * 2;
                
                // Draw a Cyan pixel (0x07FF)
                frame_buf[index] = 0x1f;
                frame_buf[index + 1] = 0xff;
            }
        }

        phase += 0.25; // Animation speed
        vTaskDelay(1);
    }
}

void gpio_setup(){
    gpio_set_function(MOSI, GPIO_FUNC_SPI); 
    gpio_set_function(SCK, GPIO_FUNC_SPI); 
    gpio_init(DC);
    gpio_init(CSN);
    gpio_init(RESET);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_set_dir(DC, GPIO_OUT);
    gpio_set_dir(CSN, GPIO_OUT);
    gpio_set_dir(RESET, GPIO_OUT);
    gpio_put(CSN, 1);
    gpio_put(RESET, 0);
    sleep_ms(100);
    gpio_put(RESET, 1);
    sleep_ms(100);
}

void dma_init(){
    dma_tx_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_tx_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true)); // Wait for SPI FIFO
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_set_irq0_enabled(dma_tx_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_callback);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_configure(
        dma_tx_chan, &c,
        &spi_get_hw(SPI_PORT)->dr, // Write to SPI Data Register
        NULL, 0, false
    );

    dma_sema = xSemaphoreCreateBinary();
}

void set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column Address Set (0x2A)
    send_command(0x2A);
    uint8_t col_data[] = {x0 >> 8,
                        x0 & 0xFF,
                        x1 >> 8,
                        x1 & 0xFF};
    send_data(col_data, sizeof(col_data));
    send_command(0x2B);
    uint8_t row_data[] = {y0 >> 8,
                        y0 & 0xFF,
                        y1 >> 8,
                        y1 & 0xFF};
    send_data(row_data, sizeof(row_data));
    // RAM Write command (0x2C) - tells the screen data is coming next
    send_command(0x2C);
}

void send_command(uint8_t cmd){
    gpio_put(DC, 0);
    gpio_put(CSN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(CSN, 1);
}

void send_data(uint8_t* data, size_t length){
    gpio_put(DC, 1);
    gpio_put(CSN, 0);
    spi_write_blocking(SPI_PORT, data, length);
    gpio_put(CSN, 1);
}

void write_frame_buffer(uint8_t* frame_buffer){
    if(xSemaphoreTake(spi_mtx, portMAX_DELAY) == pdTRUE){
        set_address_window(0, 0, 127, 159);
        gpio_put(DC, 1);
        dma_channel_set_read_addr(dma_tx_chan, frame_buffer, false);
        dma_channel_set_trans_count(dma_tx_chan, 128*160*2, false);
        gpio_put(CSN, 0);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        dma_channel_start(dma_tx_chan);

        xSemaphoreTake(dma_sema, portMAX_DELAY);
        xSemaphoreGive(spi_mtx);

    }
}