/**
  ******************************************************************************
  * @file    GPIO/GPIO_IOToggle/Src/main.c 
  * @author  MCD Application Team
  * @brief   This example describes how to configure and use GPIOs through 
  *          the STM32F3xx HAL API.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#ifdef USE_CAN
#include "CanIf.h"
#endif

#if 1
#include "trace_if.h"
#endif

/* OS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "main_application.hpp"
#include "uc_ports.hpp"


/** @addtogroup STM32F3xx_HAL_Examples
  * @{
  */

/* Private define ------------------------------------------------------------*/
#define ADCCONVERTEDVALUES_BUFFER_SIZE 256                 /* Size of array aADCxConvertedValues[] */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/


/* ADC handler declaration */


/* Variable containing ADC conversions results */
__IO uint16_t   aADCxConvertedValues[ADCCONVERTEDVALUES_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
#ifdef USE_STM32F3_DISCO
static void SystemClock_Config_STM32_F3_DISCOVERY(void);
#elif defined USE_STM32F3XX_NUCLEO_32
static void SystemClock_Config_STM32F303_NUCLEO_32(void);
#elif defined STM32F303xC
static void SystemClock_Config_STM32F303xC(void);
#elif define STM32F429xx
void SystemClock_Config_STM32F429xx(void);
#endif

static void Error_Handler(void);
//static void ADC_Config(void);

#ifdef HAL_ADC_MODULE_ENABLED
/* Private functions ---------------------------------------------------------*/
ADC_HandleTypeDef    AdcHandle;
#endif
/**
  * @brief  Main program
  * @param  None
  * @retval None
  */

extern "C"
{
    extern int _end;

    void *_sbrk(int incr)
    {
      static unsigned char *heap = NULL;
      unsigned char *prev_heap;

      if (heap == NULL) {
        heap = (unsigned char *)&_end;
      }
      prev_heap = heap;

      heap += incr;

      return prev_heap;
    }

    int _close(int file)
    {
        return -1;
    }

    int _fstat(int file, struct stat *st)
    {
        //st->st_mode = S_IFCHR;
        return 0;
    }

    int _isatty(int file)
    {
        return 1;
    }

    int _lseek(int file, int ptr, int dir)
    {
        return 0;
    }

    void _exit(int status)
    {
        __asm("BKPT #0");
    }

    void _kill(int pid, int sig)
    {
        return;
    }

    extern void *pxCurrentTCB;

    int _getpid(void)
    {
        return -1;
    }

    int _write (int file, char * ptr, int len)
    {
      int written = 0;

      if ((file != 1) && (file != 2) && (file != 3)) {
        return -1;
      }

      char print_buffer[100];
      const uint32_t u32_length =  std::min(len, 100 - 1);
      std::memcpy(print_buffer, ptr, u32_length);
      print_buffer[u32_length] = '\0';

      DEBUG_PRINTF(print_buffer);
      return u32_length;
      /*
      for (; len != 0; --len) {
        if (usart_serial_putchar(&stdio_uart_module, (uint8_t)*ptr++)) {
          return -1;
        }
        ++written;
      }
      return written;
      */
    }

    int _read (int file, char * ptr, int len)
    {
    #if 0
      int read = 0;

      if (file != 0) {
        return -1;
      }

      for (; len > 0; --len) {
        usart_serial_getchar(&stdio_uart_module, (uint8_t *)ptr++);
        read++;
      }
      return read;
    #endif
          return 0;
    }

    int _gettimeofday( struct timeval *tv, void *tzvp )
    {
        if (nullptr == tv)
        {
            return -1;
        }

        const uint64_t tickFrequency = static_cast<uint64_t>(HAL_GetTickFreq()); /* unit is Hz */

        uint64_t t = (static_cast<uint64_t>(HAL_GetTick()) * 1000 * 1000) / tickFrequency;  // get uptime in nanoseconds
        tv->tv_sec = t / 1'000'000'000;  // convert to seconds
        tv->tv_usec = ( t % 1'000'000'000 ) / 1000;  // get remaining microseconds
        return 0;  // return non-zero for error
    } // end _gettimeofday()

    void* malloc (size_t size) // _NOTHROW
    {
        /* Call the FreeRTOS version of malloc. */
        return pvPortMalloc( size );
    }
    void free (void* ptr)
    {
        /* Call the FreeRTOS version of free. */
        vPortFree( ptr );
    }

}


void * operator new( size_t size )
{
    return pvPortMalloc( size );
}

void * operator new[]( size_t size )
{
    return pvPortMalloc(size);
}

void operator delete( void * ptr )
{
    vPortFree ( ptr );
}

void operator delete[]( void * ptr )
{
    vPortFree ( ptr );
}

extern "C"
{
    void vApplicationMallocFailedHook( void )
    {
        /* vApplicationMallocFailedHook() will only be called if
        configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
        function that will get called if a call to pvPortMalloc() fails.
        pvPortMalloc() is called internally by the kernel whenever a task, queue,
        timer or semaphore is created.  It is also called by various parts of the
        demo application.  If heap_1.c or heap_2.c are used, then the size of the
        heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
        FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
        to query the size of free heap space that remains (although it does not
        provide information on how the remaining heap might be fragmented). */
        taskDISABLE_INTERRUPTS();
        for( ;; );
    }

    void vApplicationIdleHook( void )
    {
        /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
        to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
        task.  It is essential that code added to this hook function never attempts
        to block in any way (for example, call xQueueReceive() with a block time
        specified, or call vTaskDelay()).  If the application makes use of the
        vTaskDelete() API function (as this demo application does) then it is also
        important that vApplicationIdleHook() is permitted to return to its calling
        function, because it is the responsibility of the idle task to clean up
        memory allocated by the kernel to any task that has since been deleted. */
    }

    /** see https://www.cnblogs.com/shangdawei/p/4684798.html */
    void vApplicationTickHook(void)
    {
      HAL_IncTick();
    }


    void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
    {
        ( void ) pcTaskName;
        ( void ) pxTask;

        /* Run time stack overflow checking is performed if
        configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
        function is called if a stack overflow is detected. */
        taskDISABLE_INTERRUPTS();
        for( ;; );
    }
}

void MAIN_Cycle_100ms(void)
{
    using namespace std::chrono;
    app::MainApplication& o_application = app::MainApplication::get();
    int32_t counter = 0;

    std::chrono::time_point<std::chrono::system_clock> ts_last_activation = system_clock::now();

    while(true)
    {
        auto start_time = system_clock::now();

        o_application.cycle_100ms();
        counter++;
        if (counter > 10)
        {
            counter = 0;
            o_application.cycle_1000ms();
        }

        const auto delta = duration_cast<std::chrono::milliseconds>(system_clock::now() - start_time);
        auto sleep_delta = std::max(milliseconds(0), duration_cast<milliseconds>(milliseconds(100) - delta));
        //std_ex::sleep_for(std::chrono::milliseconds(100));
        sleep_delta = std::max(milliseconds(10), sleep_delta);

        const auto actual_cycle_time = duration_cast<std::chrono::milliseconds>(start_time - ts_last_activation);

        ts_last_activation = start_time;
        TRACE_LOG("SCHD", LOGLEVEL_DEBUG, "100ms sched. cycle duration %u, sleep time %u, last activation %u\n\r",
                static_cast<unsigned int>(delta.count()),
                static_cast<unsigned int>(sleep_delta.count()),
                static_cast<unsigned int>(actual_cycle_time.count()));

        std_ex::sleep_for(sleep_delta);

    }
}

void MAIN_Cycle_LowPrio_10ms(void)
{
    using namespace std::chrono;

    app::MainApplication& o_application = app::MainApplication::get();
    drivers::STM32HardwareUART* po_uart = static_cast<drivers::STM32HardwareUART*>(o_application.m_p_uart);
    while(true)
    {
        auto start_time = std::chrono::system_clock::now();


        o_application.cycle_10ms();

        // Check for data on the UART
        if (nullptr != po_uart)
        {
            po_uart->uart_process_cycle();
        }
        const auto delta = duration_cast<milliseconds>(system_clock::now() - start_time);
        auto sleep_delta = std::max(milliseconds(0), duration_cast<milliseconds>(milliseconds(10) - delta));
        sleep_delta = std::max(milliseconds(10), sleep_delta);

        TRACE_LOG("SCHD", LOGLEVEL_DEBUG, "10ms sched. cycle duration %u, sleep time %u\n\r",
                static_cast<unsigned int>(delta.count()),
                static_cast<unsigned int>(sleep_delta.count()));
        std_ex::sleep_for(sleep_delta);
    }

}

void MAIN_startup_thread(void*)
{
    // This is the initial thread running after bootup.
    /* Configure the system clock to 64 MHz */
    SystemClock_Config();

    // create our main application.
    app::MainApplication& o_application = app::MainApplication::get();
    o_application.startup_from_reset();

    TRACE_DECLARE_CONTEXT("SCHD");

    // start the cyclic thread(s)
    auto* cycle_100ms_thread = new std_ex::thread(&MAIN_Cycle_100ms, "Cycle100ms", 2u, 0x800);
    cycle_100ms_thread->detach();

    auto* m_po_io_thread = new std_ex::thread(MAIN_Cycle_LowPrio_10ms, "Cycle_LowPrio_10ms", 1u, 1024);


    while (true)
    {
        o_application.console_thread();
        // load balancing
        std_ex::sleep_for(std::chrono::milliseconds(10));

    }

    vTaskDelete(NULL);
}


int main(void)
{
    // create the port configuration object
    drivers::UcPorts* po_uc_port_configuration = new drivers::STM32F303CCT6UcPorts();

    /* STM32F3xx HAL library initialization:
       - Configure the Flash prefetch
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
    HAL_Init();

    // first thread still needs to be created with xTaskCreate, only after the scheduler has started, std::thread can be used.
    TaskHandle_t xHandle = NULL;
    xTaskCreate( MAIN_startup_thread,
                 "MAIN_startup_thread",
                 0x800,
                 NULL,
                 3u,
                 &xHandle );

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following line
    will never be reached.  If the following line does execute, then there was
    insufficient FreeRTOS heap memory available for the idle and/or timer tasks
    to be created.  See the memory management section on the FreeRTOS web site
    for more details. */

    for( ;; );
}


static void SystemClock_Config(void)
{

#ifdef USE_STM32F3_DISCO
    SystemClock_Config_STM32_F3_DISCOVERY();
#elif defined USE_STM32F3XX_NUCLEO_32
    SystemClock_Config_STM32F303_NUCLEO_32();
#elif defined STM32F303xC
    SystemClock_Config_STM32F303xC();
#elif defined STM32F429xx
    SystemClock_Config_STM32F429xx();
#else
#error "Please specify the system clock configuration for your target board."
#endif
}

#ifdef STM32_FAMILY_F3
#ifdef USE_STM32F3_DISCO
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 72000000
  *            HCLK(Hz)                       = 72000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            HSE PREDIV                     = 1
  *            PLLMUL                         = RCC_PLL_MUL9 (9)
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
static void SystemClock_Config_STM32_F3_DISCOVERY(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
  {
    Error_Handler();
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    Error_Handler();
  }
}
#endif

#ifdef USE_STM32F3XX_NUCLEO_32
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 64000000
  *            HCLK(Hz)                       = 64000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            PLLMUL                         = RCC_PLL_MUL16 (16)
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config_STM32F303_NUCLEO_32(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* HSI Oscillator already ON after system reset, activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
}
#endif

#ifdef STM32F303xC
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 64000000
  *            HCLK(Hz)                       = 64000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            PLLMUL                         = RCC_PLL_MUL16 (16)
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config_STM32F303xC(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* HSI Oscillator already ON after system reset, activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    /* Initialization Error */
    while(1);

  }
}
#endif /* STM32F303xC */
#endif

#ifdef STM32_FAMILY_F4
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 180000000
  *            HCLK(Hz)                       = 180000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 360
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
void SystemClock_Config_STM32F429xx(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* Activate the Over-Drive mode */
  HAL_PWREx_EnableOverDrive();

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}
#endif

void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
{
  /* Turn LED3 on: Transfer error in reception/transmission process */
#ifdef USE_STM32_F3_DISCO
  BSP_LED_On(LED_RED);
#elif defined USE_STM32F3XX_NUCLEO_32
  BSP_LED_On(LED_GREEN);
#endif

}


#ifdef HAL_ADC_MODULE_ENABLED
/**
  * @brief  Conversion complete callback in non blocking mode
  * @param  AdcHandle : AdcHandle handle
  * @note   This example shows a simple way to report end of conversion
  *         and get conversion result. You can add your own implementation.
  * @retval None
  */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle)
{

}

/**
  * @brief  Conversion DMA half-transfer callback in non blocking mode
  * @param  hadc: ADC handle
  * @retval None
  */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{

}

/**
  * @brief  Analog watchdog callback in non blocking mode.
  * @note:  In case of several analog watchdog enabled, if needed to know
            which one triggered and on which ADCx, check Analog Watchdog flag
            ADC_FLAG_AWD1/2/3 into HAL_ADC_LevelOutOfWindowCallback() function.
            For example:"if (__HAL_ADC_GET_FLAG(hadc1, ADC_FLAG_AWD1) != RESET)"
  * @param  hadc: ADC handle
  * @retval None
  */
  void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc)
{
}

/**
  * @brief  ADC error callback in non blocking mode
  *        (ADC conversion with interruption or transfer by DMA)
  * @param  hadc: ADC handle
  * @retval None
  */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  /* In case of ADC error, call main error handler */
  Error_Handler();
}
#endif


/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
    /* User may add here some code to deal with this error */
    while(1)
    {
#ifdef USE_STM32_F3_DISCO
        BSP_LED_Toggle(LED_RED);
#elif defined USE_STM32F3XX_NUCLEO_32
        BSP_LED_Toggle(LED_GREEN);
#endif
        HAL_Delay(1000);
    }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
