#include "pico/stdlib.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/adc.h"
#include "FreeRTOSConfig.h"
#include "stream_buffer.h"

#define ADC_PIN 27 
#define ADC_MUX (ADC_PIN-26)

TaskHandle_t adc_handle;
TaskHandle_t test_handle;
void test_task(__unused void *params);
void adc_task(__unused void *params);
void adc_isr();

static volatile uint16_t gNumSamples = 0;
static volatile StreamBufferHandle_t gxStreamHandle;


typedef struct {
    uint8_t ucNumSamples;
    uint8_t ucChannel;
    uint16_t usSampleRate;
    StreamBufferHandle_t xStreamHandle;
} adc_request_t;

int main( void )
{
    stdio_init_all();

    xTaskCreate(adc_task, "AdcTask", 1024, NULL, 1, &adc_handle); 
    xTaskCreate(test_task, "TestTask", 2048, (void*)adc_handle, 2, NULL);

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
    // xTaskNotifyGiveIndexed( adc_handle, 2);
    while(1){
        // Wait on notification from task 
        xTaskNotifyWaitIndexed(1, 0, 0, &receiveData, portMAX_DELAY);
        pxRequestStruct = (adc_request_t*)receiveData;
        printf("Received request for %d samples from channel %d at rate %d\n",
            pxRequestStruct->ucNumSamples, pxRequestStruct->ucChannel, pxRequestStruct->usSampleRate);

        ulTaskNotifyTakeIndexed(2, 1, portMAX_DELAY);
        printf("ADC Task notified that ADC is available.  Starting Request.\n");

        gNumSamples = pxRequestStruct->ucNumSamples;
        gxStreamHandle = pxRequestStruct->xStreamHandle;
        adc_select_input(pxRequestStruct->ucChannel);
        adc_set_clkdiv(48000.0f/pxRequestStruct->usSampleRate - 1);
        irq_set_enabled(ADC_IRQ_FIFO, true);
        adc_irq_set_enabled(true);
        adc_run(true);  
    }
}

void test_task(__unused void *params) 
{
    static adc_request_t xRequestStruct;
    uint8_t cntr = 0;
    // uint16_t rxBuf[255];
    xRequestStruct.ucChannel = 0;
    xRequestStruct.ucNumSamples = 250;
    xRequestStruct.usSampleRate = 1000;
    xRequestStruct.xStreamHandle = xStreamBufferCreate(250*2, 1);
    while(1){
        xTaskNotifyIndexed( (TaskHandle_t)params, 1, (uint32_t)&xRequestStruct, eSetValueWithOverwrite);

        // xStreamBufferReceive(xRequestStruct.xStreamHandle, rxBuf, 1, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(1000));
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
