/*
 * stm32_dac.cpp
 *
 *  Created on: 27.09.2019
 *      Author: Clemens
 */

#include <cstring>
#include <thread>
#include "ex_thread.hpp"
#include "stm32_uart.hpp"
#include <functional>
#include <map>
#include "trace_if.h"


#define HAS_STD_MUTEX

/* Definition for USARTx clock resources */
#define USARTx                           USART2
#define USARTx_CLK_ENABLE()              __HAL_RCC_USART2_CLK_ENABLE();
#define USARTx_RX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()
#define USARTx_TX_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()

#define USARTx_FORCE_RESET()             __HAL_RCC_USART2_FORCE_RESET()
#define USARTx_RELEASE_RESET()           __HAL_RCC_USART2_RELEASE_RESET()

namespace drivers
{
    std::map<const UART_HandleTypeDef*, STM32HardwareUART*> map_uart_handles_to_object;
    std::map<const int32_t, STM32HardwareUART*> map_uart_peripheral_id_to_object;

    STM32HardwareUART::STM32HardwareUART(GPIO_TypeDef* pt_rx_gpio_block,  uint16_t u16_rx_pin, \
            GPIO_TypeDef* pt_tx_gpio_block,  uint16_t u16_tx_pin)
    {
        TRACE_DECLARE_CONTEXT("UART");
        
        UartReady = RESET;
        m_uart_rx_interrupt_status = RESET;
        m_o_uart_handle = {};
        
        // add to map
        map_uart_handles_to_object.emplace(&this->m_o_uart_handle, this);
        // Configure pins
        GPIO_InitTypeDef  GPIO_InitStruct;

#ifdef USE_STM32_F3_DISCO
        if (pt_rx_gpio_block == GPIOD && u16_rx_pin == GPIO_PIN_6 &&
            pt_tx_gpio_block == GPIOD && u16_tx_pin == GPIO_PIN_5)
            {
            /*##-1- Enable peripherals and GPIO Clocks #################################*/
            /* Enable GPIO TX/RX clock */
            __HAL_RCC_GPIOD_CLK_ENABLE();

            /* Enable USARTx clock */
            __HAL_RCC_USART2_CLK_ENABLE();

            /*##-2- Configure peripheral GPIO ##########################################*/
            /* UART TX GPIO pin configuration  */
            GPIO_InitStruct.Pin       = u16_tx_pin;
            GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull      = GPIO_PULLUP;
            GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

            HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

            /* UART RX GPIO pin configuration  */
            GPIO_InitStruct.Pin = u16_rx_pin;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

            HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

            /*##-3- Configure the NVIC for UART ########################################*/
            /* NVIC for USART */
            HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
            HAL_NVIC_EnableIRQ(USART2_IRQn);


            // STM32F303 UART2 peripherals
            m_o_uart_handle.Instance        = USART2;
            map_uart_peripheral_id_to_object.emplace(2, this);

//#define USARTx_IRQn                      USART2_IRQn
//#define USARTx_IRQHandler                USART2_IRQHandler

        }
#elif defined USE_STM32F3XX_NUCLEO_32 || defined STM32F303xC
        if (pt_rx_gpio_block == GPIOC && u16_rx_pin == GPIO_PIN_5 &&
            pt_tx_gpio_block == GPIOC && u16_tx_pin == GPIO_PIN_4)
        {
            /*##-1- Enable peripherals and GPIO Clocks #################################*/
            /* Enable GPIO TX/RX clock */
            __HAL_RCC_GPIOC_CLK_ENABLE();

            /* Enable USARTx clock */
            __HAL_RCC_USART1_CLK_ENABLE();

            /*##-2- Configure peripheral GPIO ##########################################*/
            /* UART TX GPIO pin configuration  */
            GPIO_InitStruct.Pin       = u16_tx_pin;
            GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull      = GPIO_PULLUP;
            GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART1;

            HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

            /* UART RX GPIO pin configuration  */
            GPIO_InitStruct.Pin = u16_rx_pin;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART1;

            HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

            /*##-3- Configure the NVIC for UART ########################################*/
            /* NVIC for USART */
            HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
            HAL_NVIC_EnableIRQ(USART1_IRQn);


            // STM32F303 UART2 peripherals
            m_o_uart_handle.Instance        = USART1;
            map_uart_peripheral_id_to_object.emplace(1, this);
        }
        else if (pt_rx_gpio_block == GPIOA && u16_rx_pin == GPIO_PIN_3 &&
                pt_tx_gpio_block == GPIOA && u16_tx_pin == GPIO_PIN_2)
        {
            /*##-1- Enable peripherals and GPIO Clocks #################################*/
            /* Enable GPIO TX/RX clock */
            __HAL_RCC_GPIOA_CLK_ENABLE();

            /* Enable USARTx clock */
            __HAL_RCC_USART2_CLK_ENABLE();

            /*##-2- Configure peripheral GPIO ##########################################*/
            /* UART TX GPIO pin configuration  */
            GPIO_InitStruct.Pin       = u16_tx_pin;
            GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull      = GPIO_PULLUP;
            GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            /* UART RX GPIO pin configuration  */
            GPIO_InitStruct.Pin = u16_rx_pin;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            /*##-3- Configure the NVIC for UART ########################################*/
            /* NVIC for USART */
            HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
            HAL_NVIC_EnableIRQ(USART2_IRQn);


            // STM32F303 UART2 peripherals
            m_o_uart_handle.Instance        = USART2;
            map_uart_peripheral_id_to_object[2] = this;
        }
#elif defined STM32F429xx
        if (pt_rx_gpio_block == GPIOA && u16_rx_pin == GPIO_PIN_10 &&
            pt_tx_gpio_block == GPIOA && u16_tx_pin == GPIO_PIN_9)
        {
            /*##-1- Enable peripherals and GPIO Clocks #################################*/
            /* Enable GPIO TX/RX clock */
            __HAL_RCC_GPIOA_CLK_ENABLE();

            /* Enable USARTx clock */
            __HAL_RCC_USART1_CLK_ENABLE();

            /*##-2- Configure peripheral GPIO ##########################################*/
            /* UART TX GPIO pin configuration  */
            GPIO_InitStruct.Pin       = u16_tx_pin;
            GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull      = GPIO_PULLUP;
            GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART1;

            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            /* UART RX GPIO pin configuration  */
            GPIO_InitStruct.Pin = u16_rx_pin;
            GPIO_InitStruct.Alternate = GPIO_AF7_USART1;

            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            /*##-3- Configure the NVIC for UART ########################################*/
            /* NVIC for USART */
            HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
            HAL_NVIC_EnableIRQ(USART1_IRQn);


            // STM32F303 UART2 peripherals
            m_o_uart_handle.Instance        = USART1;
            map_uart_peripheral_id_to_object.emplace(1, this);
        }
#endif
    }

    STM32HardwareUART::~STM32HardwareUART()
    {}


    void STM32HardwareUART::connect(uint64_t u64_baud, UARTSignalWordLength en_word_length, UARTSignalStopBits en_stop_bits, \
            UARTSignalFlowControl en_flow_control, bool invert)
    {
      /*##-1- Configure the UART peripheral ######################################*/
      /* Put the USART peripheral in the Asynchronous mode (UART Mode) */
      /* UART configured as follows:
          - Word Length = 8 Bits
          - Stop Bit = One Stop bit
          - Parity = None
          - BaudRate = 9600 baud
          - Hardware flow control disabled (RTS and CTS signals) */

        m_o_uart_handle.Init.BaudRate = u64_baud;
        switch (en_word_length)
        {
        case UART_WORD_LENGTH_8BIT:
            m_o_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
        break;
        }

        switch (en_stop_bits)
        {
        case UART_STOP_BITS_2:
#ifndef STM32_FAMILY_F3
        m_o_uart_handle.Init.StopBits     = UART_STOPBITS_2;
        break;
#endif
        case UART_STOP_BITS_1_5:
#ifndef STM32_FAMILY_F4
            m_o_uart_handle.Init.StopBits     = UART_STOPBITS_1_5;
            break;
#endif
        case UART_STOP_BITS_1:
        default:
            m_o_uart_handle.Init.StopBits     = UART_STOPBITS_1;
            break;
        }

        // TODO this is just wrong
        switch (en_flow_control)
        {
        case UART_FLOW_CONTROL_NONE:
            m_o_uart_handle.Init.Parity       = UART_PARITY_NONE;
            break;
        case UART_FLOW_CONTROL_HARDWARE:
        default:
            break;

        }

        m_o_uart_handle.Init.Mode         = UART_MODE_TX_RX;
        m_o_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
#ifdef STM32_FAMILY_F3
        m_o_uart_handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
#endif


        if(HAL_UART_DeInit(&m_o_uart_handle) != HAL_OK)
        {
            Error_Handler();
        }
        if(HAL_UART_Init(&m_o_uart_handle) != HAL_OK)
        {
            Error_Handler();
        }

        m_u32_rx_buffer_usage = 0;
#ifdef DRIVER_UART_HAS_OWN_TASK
        auto main_func = std::bind(&STM32HardwareUART::uart_main, this);
        m_p_uart_buffer_thread = new std_ex::thread(main_func, "UART_RxThread", 1u, 0x600);
#endif
    }
    void STM32HardwareUART::disconnect()
    {
        // TODO
    }

    void STM32HardwareUART::update_baud_rate(uint64_t u64_baud)
    {

    }

    int STM32HardwareUART::available(void) const
    {
        return m_u32_rx_buffer_usage;
        //return m_o_uart_handle.RxXferCount;
    }

    int STM32HardwareUART::available_for_write(void) const
    {
        return 0;
    }

    int STM32HardwareUART::peek(void) const
    {
        //return m_o_uart_handle.RxXferCount;
        return m_u32_rx_buffer_usage;
    }

    int STM32HardwareUART::read(void)
    {
#if 0
        int retval = -1;
        char ai8_buffer[10];
        if(HAL_UART_Receive(&m_o_uart_handle, reinterpret_cast<uint8_t*>(ai8_buffer), 1, 100) == HAL_OK)
        {
            return static_cast<int>(ai8_buffer[0]);
        }
#else
        int retval = -1;
        if (m_u32_rx_buffer_usage > 0)
        {
            retval = m_au8_rx_buffer[0];
            m_u32_rx_buffer_usage--;
            // move the entire content... terrible design, but no time to implement
            memmove(m_au8_rx_buffer, m_au8_rx_buffer + 1, m_u32_rx_buffer_usage);
        }
        return retval;
#endif
    }

    void STM32HardwareUART::flush(void)
    {
    }

    size_t STM32HardwareUART::write(const uint8_t *a_u8_buffer, size_t size)
    {
        if (0u == size)
        {
            return 0u;
        }
        //wait for the device to be ready
        while (m_o_uart_handle.gState != HAL_UART_STATE_READY)
        {
        }

        {
            // lock the access to the hardware
#ifdef HAS_STD_MUTEX
            std::lock_guard<std::mutex> guard(this->m_o_interrupt_mutex);
#endif
            if(HAL_UART_Transmit_IT(&m_o_uart_handle, const_cast<uint8_t*>(a_u8_buffer), size) != HAL_OK)
            {
                Error_Handler();
            }

            while (UartReady != SET)
            {
            }
            UartReady = RESET;
        }
        return size;
    }

    void STM32HardwareUART::uart_process_cycle()
    {
        char ai8_buffer[10] = { 0 };

        HAL_StatusTypeDef ret_val = HAL_OK;

        while(HAL_OK == ret_val)
        {
#ifdef HAS_STD_MUTEX
            std::lock_guard<std::mutex> guard(this->m_o_interrupt_mutex);
#endif
            // only read byte-wise, horribly slow, but works for now.
            __HAL_UART_CLEAR_OREFLAG(&m_o_uart_handle);
            __HAL_UART_CLEAR_NEFLAG(&m_o_uart_handle);
            ret_val = HAL_UART_Receive(&m_o_uart_handle, reinterpret_cast<uint8_t*>(ai8_buffer), 1, 100);


            if (HAL_OK == ret_val)
            {

                if(m_u32_rx_buffer_usage < STM32UART_BUFFER_SIZE)
                {
                    m_au8_rx_buffer[m_u32_rx_buffer_usage] = ai8_buffer[0];
                    m_u32_rx_buffer_usage++;
                    m_cv_input_available.notify_all();
                }
            }
        }
    }

#ifdef DRIVER_UART_HAS_OWN_TASK
    void STM32HardwareUART::uart_main()
    {
        while(true)
        {
            uart_process_cycle();
            // load balancing
            std_ex::sleep_for(std::chrono::milliseconds(100));
        }
        //DEBUG_PRINTF("UART thread will terminate now.");
    }
#endif

    void STM32HardwareUART::Error_Handler(void) const
    {
      /* User may add here some code to deal with this error */
      while(1)
      {
#ifdef USE_STM32_F3_DISCO
          BSP_LED_Toggle(LED_RED);
#elif defined USE_STM32F3XX_NUCLEO_32
          //BSP_LED_Toggle(LED_GREEN);
#endif
        HAL_Delay(1000);
      }
    }
}


#if 0
extern "C"
{
    void HAL_UART_MspInit(UART_HandleTypeDef *huart)
    {
      GPIO_InitTypeDef  GPIO_InitStruct;

      /*##-1- Enable peripherals and GPIO Clocks #################################*/
      /* Enable GPIO TX/RX clock */
      USARTx_TX_GPIO_CLK_ENABLE();
      USARTx_RX_GPIO_CLK_ENABLE();


      /* Enable USARTx clock */
      USARTx_CLK_ENABLE();

      /*##-2- Configure peripheral GPIO ##########################################*/
      /* UART TX GPIO pin configuration  */
      GPIO_InitStruct.Pin       = USARTx_TX_PIN;
      GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
      GPIO_InitStruct.Pull      = GPIO_PULLUP;
      GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
      GPIO_InitStruct.Alternate = USARTx_TX_AF;

      HAL_GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

      /* UART RX GPIO pin configuration  */
      GPIO_InitStruct.Pin = USARTx_RX_PIN;
      GPIO_InitStruct.Alternate = USARTx_RX_AF;

      HAL_GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);
    }

    /**
      * @brief UART MSP De-Initialization
      *        This function frees the hardware resources used in this example:
      *          - Disable the Peripheral's clock
      *          - Revert GPIO configuration to their default state
      * @param huart: UART handle pointer
      * @retval None
      */
    void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
    {
      /*##-1- Reset peripherals ##################################################*/
      USARTx_FORCE_RESET();
      USARTx_RELEASE_RESET();

      /*##-2- Disable peripherals and GPIO Clocks #################################*/
      /* Configure USART2 Tx as alternate function  */
      HAL_GPIO_DeInit(USARTx_TX_GPIO_PORT, USARTx_TX_PIN);
      /* Configure USART2 Rx as alternate function  */
      HAL_GPIO_DeInit(USARTx_RX_GPIO_PORT, USARTx_RX_PIN);
    }
}
#endif

extern "C"
{
    void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
    {
      /* Set transmission flag: transfer complete */
        drivers::map_uart_handles_to_object[UartHandle]->UartReady = SET;
        //UartReady = SET;

      /* Turn LED3 on: Transfer in transmission process is correct */
      //BSP_LED_On(LED3);

    }

    /**
      * @brief  Rx Transfer completed callback
      * @param  UartHandle: UART handle
      * @note   This example shows a simple way to report end of DMA Rx transfer, and
      *         you can add your own implementation.
      * @retval None
      */
    void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
    {
      /* Set transmission flag: transfer complete */
        drivers::map_uart_handles_to_object[UartHandle]->m_uart_rx_interrupt_status = SET;
        //UartReady = SET;

      /* Turn LED5 on: Transfer in reception process is correct */
      //BSP_LED_On(LED5);

    }
    void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
    {
        TRACE_LOG("UART", LOGLEVEL_ERROR, "Hardware Error reported on UART!");
    }

    void USART1_IRQHandler(void)
    {
      HAL_UART_IRQHandler(&drivers::map_uart_peripheral_id_to_object[1]->m_o_uart_handle);
    }

    void USART2_IRQHandler(void)
    {
        if (drivers::map_uart_peripheral_id_to_object.find(2) != drivers::map_uart_peripheral_id_to_object.end())
        {
            HAL_UART_IRQHandler(&drivers::map_uart_peripheral_id_to_object[2]->m_o_uart_handle);
        }
    }
}
