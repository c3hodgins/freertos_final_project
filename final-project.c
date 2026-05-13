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
#define CSN 1
#define SCK 2
#define MOSI 3
#define DC 12
#define RESET 13

#define HEIGHT 128
#define WIDTH 160
#define BYTES_PER_PIXEL 2

// STATIC OBJECTS
SemaphoreHandle_t spi_mtx;
SemaphoreHandle_t dma_sema;
TaskHandle_t spi_handle;
BaseType_t xHigherPriorityTaskWoken;
uint8_t frame_buf2[WIDTH][HEIGHT][BYTES_PER_PIXEL];
uint8_t zero_buf[HEIGHT* BYTES_PER_PIXEL] = {0};
int dma_tx_chan;
int j = 0;
int l = 0;

// LINEAR ALGEBRA STRUCTS
typedef struct{
    int8_t x;
    int8_t y;
} point_t;

typedef struct{
    int8_t x;
    int8_t y;
    int8_t z;
} point3d_t;

// TASKS
void spi_task(void *pvParams);
void frame_buf_task(void *pvParams);
void __isr dma_callback();

// SPI DISPLAY HELPERS
void send_command(uint8_t cmd);
void send_data(uint8_t* data, size_t length);
void write_frame_buffer(uint8_t frame_buffer[WIDTH][HEIGHT][BYTES_PER_PIXEL]);
void set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// SETUP FUNCTIONS
void dma_init();
void gpio_setup();

// BASIC DRAWING FUNCTIONS
void draw_wave_160_samples(uint8_t* sample_buf, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL]);
void draw_pixel(point_t point, uint16_t val, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL]);
void draw_line(point_t p0, point_t p1, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL], uint16_t val);

// 2D LINEAR ALGEBRA FUNCTIONS
point_t rotate2d_y(point_t p, float theta);
point_t rotate2d_x(point_t p, float theta);

// 3D LINEAR ALGEBRA FUNCTIONS
point3d_t rotate3d_x(point3d_t p3, float theta);
point3d_t rotate3d_y(point3d_t p3, float theta);
point3d_t rotate3d_z(point3d_t p3, float theta);

// MAIN FUNC
int main() {
    stdio_init_all();
    gpio_setup();
    dma_init();
    spi_mtx = xSemaphoreCreateMutex();
    if (spi_mtx != NULL) {
        xTaskCreate(spi_task, "SPI_Task", 1024, NULL, 2, &spi_handle);
        xTaskCreate(frame_buf_task, "Frame_task", 256, NULL, 1, NULL);
        xTaskCreate(button_task, "ButtonTask", 256, NULL, 1, NULL);
        vTaskStartScheduler();
    }
    while (true);
}

// TASK FUNCS
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
        write_frame_buffer(frame_buf2);
        taskYIELD();
    }
}
void frame_buf_task(void *pvParams){
    float phase = 0.0;
    // uint8_t intensity;
    uint16_t offset = 0xb1fd;
    // uint8_t sin_table[160];
    // int current_val = 64;
    while(true){
        memset(frame_buf2, 0, HEIGHT*WIDTH*BYTES_PER_PIXEL);
        draw_line((point_t){0, -64}, (point_t){0, 63}, frame_buf2, 0x0841);
        draw_line((point_t){-80, 0}, (point_t){79, 0}, frame_buf2, 0x0841);
        
        point3d_t points[8] = {
            {40, 40, -40},
            {40, -40, -40}, 
            {40, -40, 40}, 
            {40, 40, 40},

            {-40, 40, -40},
            {-40, -40, -40}, 
            {-40, -40, 40}, 
            {-40, 40, 40},
            
        };
        point3d_t p_points[8];
        for(int p = 0;p < 8; p++){
            point3d_t p3d = points[p];
            p3d = rotate3d_x(p3d, phase);
            p3d = rotate3d_y(p3d, 1.5f*phase);
            p3d = rotate3d_z(p3d, 2.0f*phase);
            p_points[p] = p3d;
        }

        for(int p = 0;p < 4; p++){
            point3d_t p1 = p_points[p];
            point3d_t p2 = p_points[(p+1)%4];
            draw_line((point_t){p1.x,p1.y}, (point_t){p2.x,p2.y}, frame_buf2, offset);
        }
        for(int p = 0;p < 4; p++){
            point3d_t p1 = p_points[p+4];
            point3d_t p2 = p_points[(p+1)%4+4];
            draw_line((point_t){p1.x,p1.y}, (point_t){p2.x,p2.y}, frame_buf2, offset);
        }
        for(int p = 0;p < 4; p++){
            point3d_t p1 = p_points[p];
            point3d_t p2 = p_points[(p+4)%8];
            draw_line((point_t){p1.x,p1.y}, (point_t){p2.x,p2.y}, frame_buf2, offset);
        }
        phase+=0.01;
        // offset++;
        // draw_pixel(point, 0xffff, frame_buf2);
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}
void button_task(void *pvParams){
    const uint button_pins[] = {21, 22, 26, 27};
    for (int i = 0; i < 4; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        // Enable internal pull-up so the button connects to GND
        gpio_pull_up(button_pins[i]); 
    }
    bool last_states[4] = {true, true, true, true}; // Start unpressed (HIGH)
    while(true){
    for (int i = 0; i < 4; i++) {
                bool current_state = gpio_get(button_pins[i]);
                
                // Check for falling edge (button press)
                if (!current_state && last_states[i]) {
                    printf("Button on GP%d pressed!\n", button_pins[i]);
                    // Perform your action here
                }
                last_states[i] = current_state;
            }
            
            // Polling interval (e.g., 20ms) provides natural debouncing
            vTaskDelay(pdMS_TO_TICKS(20));
    }
}
// ISR FUNCS
void __isr dma_callback(){
    dma_hw->ints0 = 1 << dma_tx_chan;
    while (spi_is_busy(SPI_PORT));
    gpio_put(CSN, 1);
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(dma_sema, &xHigherPriorityTaskWoken);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// SETUP FUNCS
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

// SPI DISPLAY HELPER FUNCS
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
void write_frame_buffer(uint8_t frame_buffer[WIDTH][HEIGHT][BYTES_PER_PIXEL]){
    if(xSemaphoreTake(spi_mtx, portMAX_DELAY) == pdTRUE){
        set_address_window(0, 0, 127, 159);
        gpio_put(DC, 1);
        dma_channel_set_read_addr(dma_tx_chan, frame_buffer, false);
        dma_channel_set_trans_count(dma_tx_chan, 128*160*2, false);
        gpio_put(CSN, 0);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        dma_channel_start(dma_tx_chan);

        xSemaphoreTake(dma_sema, portMAX_DELAY); // Blocks current task allowing for frame_buf_task to execute
        xSemaphoreGive(spi_mtx);
    }
}

// DRAWING FUNCS
void draw_wave_160_samples(uint8_t* sample_buf, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL]){
    uint8_t y; // on range 0 to 128
    for(int x = 0; x < 160; x++){
        y = sample_buf[x] % 128;
        dst_buf[x][y][0] = 0xff;
        dst_buf[x][y][1] = 0xff;
    }
}
void draw_pixel(point_t point, uint16_t val, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL]){
    // draws pixels with origin being the center of the screen
    uint8_t x = 80+point.x;
    uint8_t y = 64+point.y;
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        dst_buf[x][y][0] = (uint8_t)(val >> 8);   // High byte
        dst_buf[x][y][1] = (uint8_t)(val & 0xFF); // Low byte
    }
}
void draw_line(point_t p0, point_t p1, uint8_t dst_buf[WIDTH][HEIGHT][BYTES_PER_PIXEL], uint16_t val) {
    int8_t x0 = p0.x, y0 = p0.y;
    int8_t x1 = p1.x, y1 = p1.y;
    int dx =  abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; // error value e_xy
    while (1) {
        point_t curr = {(int8_t)x0,(int8_t)y0};
        draw_pixel(curr, val, dst_buf); // Your function to set a pixel color
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 >= dy) { // e_xy + e_x > 0
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) { // e_xy + e_y < 0
            err += dx;
            y0 += sy;
        }
    }
}

// LINALG FUNCS
point_t rotate2d_y(point_t p, float theta) {
    // Only modifies X based on original X
    return (point_t){ (int8_t)(p.x * cosf(theta)), p.y };
}
point_t rotate2d_x(point_t p, float theta) {
    // Only modifies Y based on original Y
    return (point_t){ p.x, (int8_t)(p.y * cosf(theta)) };
}
point3d_t rotate3d_x(point3d_t p3, float theta){
    float c = cosf(theta);
    float s = sinf(theta);

    return (point3d_t){
        p3.x, 
        p3.y*c - p3.z*s, 
        p3.y*s + p3.z*c
    };
};
point3d_t rotate3d_y(point3d_t p3, float theta){
    float c = cosf(theta);
    float s = sinf(theta);

    return (point3d_t){
        p3.x*c + p3.z*s, 
        p3.y, 
        -p3.x*s + p3.z*c
    };
};
point3d_t rotate3d_z(point3d_t p3, float theta){
    float c = cosf(theta);
    float s = sinf(theta);

    return (point3d_t){
        p3.x*c - p3.y*s,
        p3.x*s + p3.y*c,
        p3.z};
};


// DIFFERENT GRAPHICS TESTS

// RANDOM NOISE ENTIRE SCREEN
// for(int k = 0; k < 160; k++) {
//     for(int i = 0; i < 128; i++) {
//         intensity = (uint8_t)(rand() % 256);
//         int index = (k * 128 + i) * 2;
//         frame_buf[index] = intensity;
//         frame_buf[index + 1] = intensity;
//     }
// }

// SIN WAVE
// for(int t = 0;t< 160;t++){
// sin_table[t] = (int)(63+(int)(64.0f*sinf(2.0f*3.14f*((float)t/160.0f)+phase)));
// }
// phase+=0.1;

// RANDOM SAMPLES    
// for(int t = 0;t< 160;t++){
    // sin_table[t] = (uint8_t)(rand() % 128);
// }

// RANDOM WALK
// for(int t = 0;t< 160;t++){
//     current_val += (rand() % 3) - 1;

//     if(current_val < 0) current_val = 0;
//     if(current_val > 127) current_val = 127;

//     sin_table[t] = (uint8_t)current_val;
//     // sin_table[t] = (uint8_t)(64 + rand()%10 - 5);
// }
// draw_wave_160_samples(sin_table);
// draw_wave_160_samples(grid);
