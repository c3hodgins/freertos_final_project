#include "pico/stdlib.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/adc.h"
#include "FreeRTOSConfig.h"
#include "stream_buffer.h"
#define TLS_RAND_INDEX 0

#define ADC_PIN 27 
#define ADC_MUX (ADC_PIN-26)

TaskHandle_t adc_handle;
TaskHandle_t test_handle;
void test_task(__unused void *params);
void adc_task(__unused void *params);
void adc_isr();
int tls_rand();
void tls_srand(uint32_t seed);

static volatile uint16_t gNumSamples = 0;
static volatile StreamBufferHandle_t gxStreamHandle;
static uint8_t task1_idx = 1;
static uint8_t task2_idx = 2;
static uint8_t task3_idx = 3;


typedef struct {
    uint8_t ucNumSamples;
    uint8_t ucChannel;
    uint16_t usSampleRate;
    StreamBufferHandle_t xStreamHandle;
} adc_request_t;

int main( void )
{
    stdio_init_all();
    xTaskCreate(adc_task, "AdcTask", 2048, NULL, 2, &adc_handle); 
    xTaskCreate(test_task, "TestTask", 2048, &task1_idx , 1, NULL);
    // xTaskCreate(test_task, "TestTask", 2048, &task2_idx , 1, NULL);
    // xTaskCreate(test_task, "TestTask", 2048, &task3_idx , 1, NULL);
    
    vTaskStartScheduler();
}

void adc_task(__unused void *params){
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_MUX);
    adc_fifo_setup(true, true, 8, false,false);
    adc_irq_set_enabled(true);
    adc_set_clkdiv(48000000/1000-1);
    adc_run(true);

    irq_set_priority(ADC_IRQ_FIFO,0x80);
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_isr);
    irq_set_enabled(ADC_IRQ_FIFO, true);
    adc_irq_set_enabled(true);
    
    adc_request_t* pxRequestStruct;
    uint32_t receiveData;
    uint16_t adc_val;
    uint8_t cnt = 0;

    while(1){
        cnt++;
        xTaskNotifyWaitIndexed(1, 0, 0xFFFFFFFF, &receiveData, portMAX_DELAY);
        pxRequestStruct = (adc_request_t*)receiveData;
        printf("Received request %d for %d samples from channel %d at rate %d\n", cnt, 
            pxRequestStruct->ucNumSamples, pxRequestStruct->ucChannel, pxRequestStruct->usSampleRate);

        ulTaskNotifyTakeIndexed(2, 1, portMAX_DELAY);
        printf("ADC Task notified that ADC is available.  Starting Request.\n");

        gNumSamples = pxRequestStruct->ucNumSamples;
        gxStreamHandle = pxRequestStruct->xStreamHandle;
        adc_fifo_drain();
        adc_select_input(pxRequestStruct->ucChannel);
        adc_set_clkdiv(48000000.0f/pxRequestStruct->usSampleRate - 1);
        irq_set_enabled(ADC_IRQ_FIFO, true);
        adc_irq_set_enabled(true);
        adc_run(true);  
    }
}

void test_task(__unused void *params) 
{
    adc_request_t xRequestStruct;
    uint8_t cntr = 0;
    uint16_t rxBuf[255];
    xRequestStruct.ucChannel = 0;
    xRequestStruct.ucNumSamples = 250;
    xRequestStruct.usSampleRate = 1000;
    xRequestStruct.xStreamHandle = xStreamBufferCreate(512, 1);
    tls_srand(*(uint8_t*)params);
    
    while(1){
        uint8_t ucRandChannel = tls_rand()%5;
        uint16_t ucRandomDelay = (uint16_t)(((float)tls_rand()/32767.0f) * (5000.0f) + 1.0f);
        uint8_t ucRandNumSamples = tls_rand()%256;
        uint16_t usRandSampleRate = (uint16_t)(((float)tls_rand() / 32767.0f) * (50000.0f - 750.0f)) + 750.0f;
        xRequestStruct.ucNumSamples = ucRandNumSamples;
        xRequestStruct.ucChannel = ucRandChannel;
        xRequestStruct.usSampleRate = usRandSampleRate;
        xStreamBufferSetTriggerLevel(xRequestStruct.xStreamHandle, 2*xRequestStruct.ucNumSamples);
        xTaskNotifyIndexed((TaskHandle_t)adc_handle, 1, (uint32_t)&xRequestStruct, eSetValueWithOverwrite);
        printf("Task %d Requesting %d samples from channel %d at rate %d\n", 
            *(uint8_t*)params, ucRandNumSamples, ucRandChannel, usRandSampleRate);
        xStreamBufferReceive(xRequestStruct.xStreamHandle, rxBuf, 2*xRequestStruct.ucNumSamples, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(ucRandomDelay));
    }
}

void adc_isr()
{
    BaseType_t HighPrioTaskWoken = false;
    uint16_t usAdcBuffer[8];
    uint8_t i = 0;
    while( !adc_fifo_is_empty() && gNumSamples > 0){
        usAdcBuffer[i++] = adc_fifo_get();
        gNumSamples--;
    }
    if (i > 0) {
        xStreamBufferSendFromISR(gxStreamHandle, usAdcBuffer, i * 2, &HighPrioTaskWoken);
    }
    if (gNumSamples == 0){
        adc_run(false);
        adc_irq_set_enabled(false);
        irq_set_enabled(ADC_IRQ_FIFO, false);
        vTaskNotifyGiveIndexedFromISR( adc_handle, 2,
            &HighPrioTaskWoken);

        gNumSamples = -1;
    }
    portYIELD_FROM_ISR( HighPrioTaskWoken);
}

int tls_rand()
{ 
    uint32_t seed= (uint32_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_RAND_INDEX);
    seed = (seed * 1103515245 + 12345);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_RAND_INDEX, (void *)seed);
    return (int)((seed >> 16) & 0x7FFF);
}

void tls_srand(uint32_t seed)
{ 
    vTaskSetThreadLocalStoragePointer(NULL, TLS_RAND_INDEX, (void *)seed);
}