/*
 * Copyright 2016-2023 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    MK64F12_Project.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_debug_console.h"
/* TODO: insert other include files here. */

/* TODO: insert other definitions and declarations here. */

#define BOARD_LED_GPIO     BOARD_LED_RED_GPIO
#define BOARD_LED_GPIO_PIN BOARD_LED_RED_PIN
#define RX_MESSAGE_BUFFER_NUM (0)
#define TX_MESSAGE_BUFFER_NUM (1)
#define RX_ID 0x10
#define TX_ID 0x123

void PIT_CHANNEL_0_IRQHANDLER(void);
void CAN0_CAN_ORED_MB_IRQHANDLER(void);


flexcan_frame_t txFrame, rxFrame;
flexcan_mb_transfer_t txXfer, rxXfer;
//flexcan_frame_t frame;
flexcan_rx_mb_config_t mbConfig;
uint8_t rxComplete = 0, txComplete = 0, counter = 0, txFlag = 0;
uint16_t waitTX = 1000, waitRX = 40,counterTime = 5;
status_t statusss;
bool TXFLAGWAIT = false;
/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    FLEXCAN_EnableMbInterrupts(CAN0, 1 << 0);
    FLEXCAN_EnableMbInterrupts(CAN0, 1 << 1);
    LED_RED_INIT(0);
    LED_GREEN_INIT(0);
    LED_BLUE_INIT(0);

#ifndef BOARD_INIT_DEBUG_CONSOLE_PERIPHERAL
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();
#endif
    // config TX CAN
    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.id = FLEXCAN_ID_STD(TX_ID);
    txFrame.type = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.length = (uint8_t)2; // DLC
    txFrame.dataByte0 = (uint8_t)5;
    txFrame.dataByte1 = (uint8_t)5;
    txFrame.length = 2;


    PRINTF("Hello World\n");
    statusss = FLEXCAN_WriteTxMb(CAN0, 1, &txFrame);

    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    while(1) {
        i++ ;
        if(!waitTX && !TXFLAGWAIT && txFlag){
        	//counterTime += 2;
        	txFrame.dataByte0 = (uint8_t)((counterTime+20)/10);
        	LED_RED_TOGGLE();
        	txFlag = 0;
        	TXFLAGWAIT = true;
        	statusss = FLEXCAN_WriteTxMb(CAN0, 1, &txFrame); // MANDO Y ESPERO A VER QUE ONDA
        }

        if(rxComplete){
        	counterTime=rxFrame.dataByte0*10;
        	txFlag = 1;
        	rxComplete = 0;
        }
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */
        __asm volatile ("nop");
    }
    return 0 ;
}

/* PIT0_IRQn interrupt handler */
void PIT_CHANNEL_0_IRQHANDLER(void) {
  uint32_t intStatus;
  /* Reading all interrupt flags of status register */
  intStatus = PIT_GetStatusFlags(PIT_PERIPHERAL, PIT_CHANNEL_0);
  PIT_ClearStatusFlags(PIT_PERIPHERAL, PIT_CHANNEL_0, intStatus);

  /* Place your code here */
  if(counter)
	  counter--;
  if(waitTX)
	  waitTX--;
  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}



/* CAN0_ORed_Message_buffer_IRQn interrupt handler */
void CAN0_CAN_ORED_MB_IRQHANDLER(void) {
  /*  Place your code here */
	uint32_t test1, test2;
	test1 = FLEXCAN_GetMbStatusFlags(CAN0, 1 << 0);
	test2 = FLEXCAN_GetMbStatusFlags(CAN0, 1 << 1);
	if (0U != FLEXCAN_GetMbStatusFlags(CAN0, 1 << 0)){
		FLEXCAN_ClearMbStatusFlags(CAN0, 1 << 0);
		FLEXCAN_ReadRxMb(CAN0, 0, &rxFrame); //leermos datos
		//if(rxFrame.length == 2){ // 2 bytes == transmision completada.
		rxComplete = 1;
		LED_BLUE_TOGGLE();
		//}
	}
	if (0U != FLEXCAN_GetMbStatusFlags(CAN0, 1 << 1)){
		FLEXCAN_ClearMbStatusFlags(CAN0, 1 << 1);
		//FLEXCAN_ReadRxMb(CAN0, 0, &rxFrame); //leermos datos

		//if(rxFrame.length == 2){ // 2 bytes == transmision completada.
		if(statusss == kStatus_Success) //se envio todo ok
			txComplete = 1;
		else {
			if(statusss == kStatus_Fail){ //hubo falla
				//retry
			}

		}
		LED_GREEN_TOGGLE();
		waitTX = 1000;
		TXFLAGWAIT = false;
		//}
	}
  /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F
     Store immediate overlapping exception return operation might vector to incorrect interrupt. */
  #if defined __CORTEX_M && (__CORTEX_M == 4U)
    __DSB();
  #endif
}

