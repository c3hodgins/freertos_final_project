#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* CLOCK SETTINGS */
#define configCPU_CLOCK_HZ                                          150000000
#define configTICK_RATE_HZ                                          100

/* SCHEDULER RELATED */
#define configUSE_PREEMPTION                                        1                  
#define configUSE_TIME_SLICING                                      1
#define configUSE_16_BIT_TICKS                                      0
#define configUSE_TICK_HOOK                                         0
#define configSTACK_DEPTH_TYPE                                      uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE                            size_t
#define configMAX_TASK_NAME_LEN                                     16
#define configUSE_PORT_OPTIMISED_TASK_SELECTION                     1
#define configMAX_PRIORITIES                                        4
#define configCHECK_FOR_STACK_OVERFLOW                              0
#define configUSE_DAEMON_TASK_STARTUP_HOOK                          0
#define configUSE_CO_ROUTINES                                       0   
#define configMAX_CO_ROUTINE_PRIORITIES                             1
#define configENABLE_FPU                                            0

/* Memory allocation related definitions. */
#define configHEAP_CLEAR_MEMORY_ON_FREE                             0
#define configSUPPORT_STATIC_ALLOCATION                             0
#define configSUPPORT_DYNAMIC_ALLOCATION                            1
#define configTOTAL_HEAP_SIZE                                       (128*1024)
#define configAPPLICATION_ALLOCATED_HEAP                            0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP                   0
#define configUSE_MALLOC_FAILED_HOOK                                0

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 0
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          0
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_uxTaskGetStackHighWaterMark2    0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              0

/* SYNCHRONIZATION PROPERTIES*/
#define configUSE_TASK_NOTIFICATIONS                                1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES                       5
#define configUSE_MUTEXES                                           1
#define configUSE_RECURSIVE_MUTEXES                                 0
#define configUSE_COUNTING_SEMAPHORES                               1
#define configQUEUE_REGISTRY_SIZE                                   8
#define configUSE_QUEUE_SETS                                        0
#define configUSE_NEWLIB_REENTRANT                                  0
#define configENABLE_BACKWARD_COMPATIBILITY                         0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS                     10
#define configUSE_MINI_LIST_ITEM                                    1
#define configUSE_SB_COMPLETED_CALLBACK                             0

/* IDLE TASK SETTINGS*/
#define configIDLE_SHOULD_YIELD                                     1
#define configUSE_IDLE_HOOK                                         0
#define configMINIMAL_STACK_SIZE                                    512


/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS                       0
#define configUSE_TRACE_FACILITY                            0
#define configUSE_STATS_FORMATTING_FUNCTIONS                0

/* Co-routine related definitions. */

/* Software timer related definitions. */
#define configUSE_TIMERS                                    1
#define configTIMER_TASK_PRIORITY                           ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                            10
#define configTIMER_TASK_STACK_DEPTH                        1024
#define configUSE_DAEMON_TASK_STARTUP_HOOK                  0
#define INCLUDE_xTimerPendFunctionCall                      1


/* Interrupt nesting behaviour configuration. */
/*
#define configKERNEL_INTERRUPT_PRIORITY         [dependent of processor]
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    [dependent on processor and application]
#define configMAX_API_CALL_INTERRUPT_PRIORITY   [dependent on processor and application]
*/

/* Define to trap errors during development. */
#define configASSERT( x ) assert()

/*NOT YET AVAILABLE OR STABLE ON PICO2*/
#define configUSE_TICKLESS_IDLE                                     0
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    16


#endif
