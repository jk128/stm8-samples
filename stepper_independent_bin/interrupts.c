/*
 * interrupts.c
 *
 * Copyright 2014 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ports_definition.h"
#include "main.h"
#include "stepper.h"

#define TREG(N, R)	TIM##N##_##R

#define STPR_INTR(X)						\
	if(TREG(X, SR1) & TIM_SR1_UIF){			\
		TREG(X, SR1) &= ~TIM_SR1_UIF;		\
		if(Nsteps[X-1]){					\
			if(++usteps[X-1] == USteps){	\
				usteps[X-1] = 0;			\
				if(!StepperInfty)			\
				if(--Nsteps[X-1] == 0){		\
					stop_motor(X-1);		\
				}							\
			}								\
		}									\
	}


// Top Level Interrupt
INTERRUPT_HANDLER(TLI_IRQHandler, 0){}

// Auto Wake Up Interrupt
INTERRUPT_HANDLER(AWU_IRQHandler, 1){}

// Clock Controller Interrupt
INTERRUPT_HANDLER(CLK_IRQHandler, 2){}

// External Interrupt PORTA
INTERRUPT_HANDLER(EXTI_PORTA_IRQHandler, 3){
	check_EP();
}

// External Interrupt PORTB
INTERRUPT_HANDLER(EXTI_PORTB_IRQHandler, 4){
	check_EP();
}

// External Interrupt PORTC
INTERRUPT_HANDLER(EXTI_PORTC_IRQHandler, 5){
	check_EP();
}

// External Interrupt PORTD
INTERRUPT_HANDLER(EXTI_PORTD_IRQHandler, 6){
	check_EP();
}

// External Interrupt PORTE
INTERRUPT_HANDLER(EXTI_PORTE_IRQHandler, 7){
	check_EP();
}

#ifdef STM8S903
// External Interrupt PORTF
INTERRUPT_HANDLER(EXTI_PORTF_IRQHandler, 8){}
#endif // STM8S903

#if defined (STM8S208) || defined (STM8AF52Ax)
// CAN RX Interrupt routine.
INTERRUPT_HANDLER(CAN_RX_IRQHandler, 8){}

// CAN TX Interrupt routine.
INTERRUPT_HANDLER(CAN_TX_IRQHandler, 9){}
#endif // STM8S208 || STM8AF52Ax

// SPI Interrupt routine.
INTERRUPT_HANDLER(SPI_IRQHandler, 10){}

// Timer1 Update/Overflow/Trigger/Break Interrupt
INTERRUPT_HANDLER(TIM1_UPD_OVF_TRG_BRK_IRQHandler, 11){
	STPR_INTR(1);
}

// Timer1 Capture/Compare Interrupt routine.
INTERRUPT_HANDLER(TIM1_CAP_COM_IRQHandler, 12){}

#ifdef STM8S903
// Timer5 Update/Overflow/Break/Trigger Interrupt
INTERRUPT_HANDLER(TIM5_UPD_OVF_BRK_TRG_IRQHandler, 13){}

// Timer5 Capture/Compare Interrupt
INTERRUPT_HANDLER(TIM5_CAP_COM_IRQHandler, 14){}

#else // STM8S208, STM8S207, STM8S105 or STM8S103 or STM8AF62Ax or STM8AF52Ax or STM8AF626x
// Timer2 Update/Overflow/Break Interrupt
INTERRUPT_HANDLER(TIM2_UPD_OVF_BRK_IRQHandler, 13){ // generate pulses for stepper CLK
	STPR_INTR(2);
}

// Timer2 Capture/Compare Interrupt
INTERRUPT_HANDLER(TIM2_CAP_COM_IRQHandler, 14){
}
#endif // STM8S903

#if defined (STM8S208) || defined(STM8S207) || defined(STM8S007) || defined(STM8S105) || \
    defined(STM8S005) ||  defined (STM8AF62Ax) || defined (STM8AF52Ax) || defined (STM8AF626x)
// Timer3 Update/Overflow/Break Interrupt
INTERRUPT_HANDLER(TIM3_UPD_OVF_BRK_IRQHandler, 15){
	STPR_INTR(3);
}

// Timer3 Capture/Compare Interrupt
INTERRUPT_HANDLER(TIM3_CAP_COM_IRQHandler, 16){}
#endif // STM8S208, STM8S207 or STM8S105 or STM8AF62Ax or STM8AF52Ax or STM8AF626x

#if defined (STM8S208) || defined(STM8S207) || defined(STM8S007) || defined(STM8S103) || \
    defined(STM8S003) ||  defined (STM8AF62Ax) || defined (STM8AF52Ax) || defined (STM8S903)
// UART1 TX Interrupt
INTERRUPT_HANDLER(UART1_TX_IRQHandler, 17){}

// UART1 RX Interrupt
INTERRUPT_HANDLER(UART1_RX_IRQHandler, 18){}
#endif // STM8S208 or STM8S207 or STM8S103 or STM8S903 or STM8AF62Ax or STM8AF52Ax

// I2C Interrupt
INTERRUPT_HANDLER(I2C_IRQHandler, 19){}

#if defined(STM8S105) || defined(STM8S005) ||  defined (STM8AF626x)
// UART2 TX interrupt
INTERRUPT_HANDLER(UART2_TX_IRQHandler, 20){}

// UART2 RX interrupt
INTERRUPT_HANDLER(UART2_RX_IRQHandler, 21){
	U8 rb;
	if(UART2_SR & UART_SR_RXNE){ // data received
		rb = UART2_DR; // read received byte & clear RXNE flag
		//while(!(UART2_SR & UART_SR_TXE));
		// get marked byte?
		if(UART2_CR1 & UART_CR1_R8){ // Master wanna change device?
			if(rb != UART_devNUM){ // another device number
				UART_is_our = 0;
				UART2_CR2 &= ~UART_CR2_TEN; // disable transmitter
			}else{ // our device
				UART_is_our = 1;
				UART2_CR2 |= UART_CR2_TEN; // enable transmitter
				UART_send_byte('*'); // send '*' - we got the command
			}
			return;
		}
		if(!UART_is_our) return; // this isn't our business - !marked & !our
		UART_send_byte(rb); // echo received symbol
		UART_rx[UART_rx_cur_i++] = rb; // put received byte into cycled buffer
		if(UART_rx_cur_i == UART_rx_start_i){ // Oops: buffer overflow! Just forget old data
			UART_rx_start_i++;
			check_UART_pointer(UART_rx_start_i);
		}
		check_UART_pointer(UART_rx_cur_i);
	}
}
#endif // STM8S105 or STM8AF626x

#if defined(STM8S207) || defined(STM8S007) || defined(STM8S208) || defined (STM8AF52Ax) || defined (STM8AF62Ax)
// UART3 TX interrupt
INTERRUPT_HANDLER(UART3_TX_IRQHandler, 20){}

// UART3 RX interrupt
INTERRUPT_HANDLER(UART3_RX_IRQHandler, 21){}
#endif // STM8S208 or STM8S207 or STM8AF52Ax or STM8AF62Ax

#if defined(STM8S207) || defined(STM8S007) || defined(STM8S208) || defined (STM8AF52Ax) || defined (STM8AF62Ax)
// ADC2 interrupt
INTERRUPT_HANDLER(ADC2_IRQHandler, 22){}
#else
// ADC1 interrupt
INTERRUPT_HANDLER(ADC1_IRQHandler, 22){
}
#endif // STM8S208 or STM8S207 or STM8AF52Ax or STM8AF62Ax

#ifdef STM8S903
// Timer6 Update/Overflow/Trigger Interrupt
INTERRUPT_HANDLER(TIM6_UPD_OVF_TRG_IRQHandler, 23){}
#else // STM8S208, STM8S207, STM8S105 or STM8S103 or STM8AF52Ax or STM8AF62Ax or STM8AF626x
// Timer4 Update/Overflow Interrupt
INTERRUPT_HANDLER(TIM4_UPD_OVF_IRQHandler, 23){
	if(TIM4_SR & TIM_SR1_UIF){ // update interrupt
		Global_time++; // increase timer
	}
	TIM4_SR = 0; // clear all interrupt flags
}
#endif // STM8S903

// Eeprom EEC Interrupt
INTERRUPT_HANDLER(EEPROM_EEC_IRQHandler, 24){}
