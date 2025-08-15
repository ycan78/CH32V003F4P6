/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2023/12/25
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 *Multiprocessor communication mode routine:
 *Master:USART1_Tx(PD5)\USART1_Rx(PD6).
 *This routine demonstrates that USART1 receives the data sent by CH341 and inverts
 *it and sends it (baud rate 115200).
 *
 *Hardware connection:PD5 -- Rx
 *                     PD6 -- Tx
 *
 */

#include "debug.h"
#include <stdio.h>
#include <string.h>
     // device-specific register definitions



/* Global define */


/* Global Variable */
volatile uint8_t buttonPressed=0, Invalid;
uint8_t is_working=0;
uint32_t last_time=0;
char commandBuffer[10];
volatile char usartRxBuf[16];
volatile uint8_t usartRxIdx = 0;
volatile uint8_t usartCmdReady = 0;


volatile uint8_t encoderClicked = 0, pin4=0, pin5=0, temp, temp2, prevState  ; //flag
volatile int32_t encoderClicks = 0; 
/*********************************************************************
 * @fn      USARTx_CFG
 *
 * @brief   Initializes the USART2 & USART3 peripheral.
 *
 * @return  none
 */
void USARTx_CFG(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    /* USART1 TX-->D.5   RX-->D.6 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef n = {0};
    n.NVIC_IRQChannel = USART1_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority = 0;
    n.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&n);

}

#include "debug.h"








void initializeGPIOs(){
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    EXTI_InitTypeDef  EXTI_InitStructure = {0};
    NVIC_InitTypeDef  NVIC_InitStructure ={0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_GPIOD |
                           RCC_APB2Periph_GPIOC, ENABLE);

    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;; 
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource4);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource5);

    
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
   EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;

    EXTI_InitStructure.EXTI_Line    = EXTI_Line4;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    EXTI_InitStructure.EXTI_Line    = EXTI_Line5;
    EXTI_Init(&EXTI_InitStructure);

    
    NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}



void receiveCommandUSART(char *commandBuffer)
{
    uint8_t index = 0;
    char receivedCharacter;
    memset(commandBuffer, 0, 10);

    while (index < (10 - 1))
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
        receivedCharacter = USART_ReceiveData(USART1); 

        if (receivedCharacter == '\n' || receivedCharacter == '\r') 
        {
            break;
        }
        commandBuffer[index++] = receivedCharacter;
    }
    commandBuffer[index] = '\0';
}


void processCommandUSART(const char *command)
{
    if (command[0] == 'M' && strlen(command) >= 2)
    {
        char commandNumber= command[1];  
        int commandIndex= commandNumber-'0'; 

        if (commandIndex>= 1 && commandIndex<= 3) 
        {
            switch (commandIndex) {
                case 1:
                    GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_RESET);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_RESET);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_RESET);
                    break;
                case 2:

                    
                    GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_SET);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_SET);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_RESET);
                    break;
                case 3:
                    printf("case 3");
                    GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_RESET);
                    Delay_Ms(5);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_RESET);
                    GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_SET);
                    Delay_Ms(5);
                    GPIO_WriteBit(GPIOC, GPIO_Pin_1, Bit_SET);
                    break;
                
            }
        }
    }
}



/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
   
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    USART_Printf_Init(115200);
    
    printf("SystemClk:%d\r\n",SystemCoreClock);
    printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    initializeGPIOs();
    prevState = (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) << 1) | GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5);

    USARTx_CFG();
  
    while(1)
    {
        
        if(encoderClicked==1){
            if(Invalid==1){
                printf("!!INVALID!!\n");
                Invalid=0;
            }       
            encoderClicked=0;
            printf("STATE: %d%d, CLICKS: %d \n", pin4, pin5,encoderClicks);

            
        }
        
        
       
       if (usartCmdReady) {
        
        strcpy(commandBuffer, (const char*)usartRxBuf);
        processCommandUSART(commandBuffer);
        usartCmdReady = 0;
    }

    }


    
}


void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI7_0_IRQHandler(void)
{
    
    encoderClicked=1;
    uint8_t A = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4);
    uint8_t B = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5);
    uint8_t state = (A << 1) | B;
    pin4=A;
    pin5=B;
    if ((prevState == 0 && state == 1) ||
        (prevState == 1 && state == 3) ||
        (prevState == 3 && state == 2) ||
        (prevState == 2 && state == 0))
        encoderClicks++;
    else if ((prevState == 0 && state == 2) ||
             (prevState == 2 && state == 3) ||
             (prevState == 3 && state == 1) ||
             (prevState == 1 && state == 0))
        encoderClicks--;
    else{
        Invalid=1;
    }
    prevState = state;

    EXTI_ClearITPendingBit(EXTI_Line4 | EXTI_Line5);
}


void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
  
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t ch = USART_ReceiveData(USART1); 

        if (!usartCmdReady) {
            if (ch == '\r' || ch == '\n') {
                usartRxBuf[usartRxIdx] = '\0';
                usartCmdReady = 1;
                usartRxIdx = 0;
            } else if (usartRxIdx < sizeof(usartRxBuf) - 1) {
                usartRxBuf[usartRxIdx++] = ch;
            } else {
                
                usartRxIdx = 0;
            }
        }
    }

   
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        (void)USART1->STATR;
        (void)USART1->DATAR;
    }
}