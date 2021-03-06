		ifndef	__stm8s005c6inc	; avoid multiple inclusion
__stm8s005c6inc	equ	1

		save
		listing	off		; no listing over this file

;****************************************************************************
;*                                                                          *
;*   AS 1.42 - File REG005C6.INC                                            *
;*                                                                          *
;*   contains SFR and Bit Definitions for STM8S005C6/STM8S005K6             *
;*   source: DS8638 Rev 5                                                   *
;*                                                                          *
;****************************************************************************

;----------------------------------------------------------------------------
; Memory Addresses

E2START		label	$4000		; start address internal EEPROM
E2END		label	E2START+127	; end     "        "       "

FLASHSTART	label	$8000		; start address internal Flash

BLSTART		label	$6000		; start address boot loader
BLEND		label	$67ff		; end     "      "     "

RAMSTART	label	$0000		; start address internal RAM
RAMEND		label	$07ff		; end     "        "      "

;----------------------------------------------------------------------------
; Option Bytes

OPT0		label	$4800		; Read-out protection
ROP		bfield	OPT0,0,8
OPT1		label	$4801		; User boot code
UBC		bfield	OPT1,0,8
NOPT1		label	$4802
NUBC		bfield	NOPT1,0,8
OPT2		label	$4803		; Alternate function remapping
AFR		bfield	OPT2,0,8
NOPT2		label	$4804
NAFR		bfield	NOPT2,0,8
OPT3		label	$4805		; Misc. option
OPT_HSITRIM	bit	OPT3,4
LSI_EN		bit	OPT3,3
IWDG_HW		bit	OPT3,2
WWDG_HW		bit	OPT3,1
WWDG_HALT	bit	OPT3,0
NOPT3		label	$4806
NHSITRIM	bit	NOPT3,4
NLSI_EN		bit	NOPT3,3
NIWDG_HW	bit	NOPT3,2
NWWDG_HW	bit	NOPT3,1
NWWDG_HALT	bit	NOPT3,0
OPT4		label	$4807		; Clock option
EXTCLK		bit	OPT4,3
CKAWUSEL	bit	OPT4,2
PRSC1		bit	OPT4,1
PRSC0		bit	OPT4,0
NOPT4		label	$4808
NEXTCLK		bit	NOPT4,3
NCKAWUSEL	bit	NOPT4,2
NPRSC1		bit	NOPT4,1
NPRSC0		bit	NOPT4,0
OPT5		label	$4809		; HSE clock startup
HSECNT		bfield	OPT5,0,8
NOPT5		label	$480a
NHSECNT		bfield	NOPT5,0,8
OPT6		label	$480b		; Reserved
NOPT6		label	$480c
OPT7		label	$480d		; Flash wait states
NOPT7		label	$480e
OPTBL		label	$487e		; Boot Loader
BL		bfield	OPTBL,0,8
NOPTBL		label	$487f
NBL		bfield	NOPTBL,0,8

;----------------------------------------------------------------------------
; Vectors

RESET_vect	label	$8000		; Reset
TRAP_vect	label	$8004		; Software interrupt
TLI_vect	label	$8008		; External top level interrupt
AWU_vect	label	$800c		; Auto wake up from halt
CLK_vect	label	$8010		; Clock controller
EXTI0_vect	label	$8014		; Port A external interrupts
EXTI1_vect	label	$8018		; Port B external interrupts
EXTI2_vect	label	$801c		; Port C external interrupts
EXTI3_vect	label	$8020		; Port D external interrupts
EXTI4_vect	label	$8024		; Port E external interrupts
SPI_vect	label	$8030		; End of transfer
TIM1_vect	label	$8034		; TIM1 update/overflow/underflow/trigger/break
TIM1_CAPT_vect	label	$8038		; TIM1 capture/compare
TIM2_vect	label	$803c		; TIM2 update /overflow
TIM2_CAPT_vect	label	$8040		; TIM2 capture/compare
TIM3_vect	label	$8044
TIM3_CAPT_vect	label	$8048
I2C_vect	label	$8054		; I2C interrupt
UART2_TX_vect	label	$8058		; Tx complete
UART2_RX_vect	label	$805c		; Receive register DATA FULL
ADC1_vect	label	$8060		; ADC1 end of conversion/analog watchdog interrupt
TIM4_vect	label	$8064		; TIM4 update/overflow
FLASH_vect	label	$8068		; EOP/WR_PG_DIS

;----------------------------------------------------------------------------
; GPIO

		include	"gpio.inc"
		__defgpio "PA",$5000
		__defgpio "PB",$5005
		__defgpio "PC",$500a
		__defgpio "PD",$500f
		__defgpio "PE",$5014
		__defgpio "PF",$5019
		__defgpio "PG",$500e
		__defgpio "PH",$5023
		__defgpio "PI",$5028

;----------------------------------------------------------------------------
; Flash

		include	"flash.inc"
		__defflash $505a

;----------------------------------------------------------------------------
; Interrupt Controller

		include	"itc.inc"
		__defexti $50a0,6
		__defitc $7f70,30

;----------------------------------------------------------------------------
; Reset Controller

		include	"rst.inc"
		__defrst $50b3

;----------------------------------------------------------------------------
; Clock Controller

		include	"clk.inc"
		__defclk $50c0

;----------------------------------------------------------------------------
; Window Watchdog

		include	"wwdg.inc"
		__defwwdg $50d1

;----------------------------------------------------------------------------
; Independent Watchdog

		include	"iwdg.inc"
		__defiwdg $50e0

;----------------------------------------------------------------------------
; Beeper

		include	"beep.inc"
		__defbeep $50f3

;----------------------------------------------------------------------------
; Serial Peripheral Interface

		include	"spi.inc"
		__defspi $5200

;----------------------------------------------------------------------------
; I2C

		include	"i2c.inc"
		__defi2c $5210

;----------------------------------------------------------------------------
; UART2

		include "uart2.inc"
		__defusart2 "UART2",$5240

;----------------------------------------------------------------------------
; Timer 1

		include	"tim1.inc"
		__deftim1 $5250

;----------------------------------------------------------------------------
; Timer 2

		include	"tim2.inc"
		__deftim2 $5300,0

;----------------------------------------------------------------------------
; Timer 3

		include	"tim3.inc"
		__deftim3 $5320,0

;----------------------------------------------------------------------------
; Timer 4

		include	"tim4.inc"
		__deftim4 $5340,0

;----------------------------------------------------------------------------
; A/D Converter 1

		include "adc1.inc"
		__defadc1 "ADC",,$5400

;----------------------------------------------------------------------------
; CPU

		include	"stm8/cpuregs.inc"
		__defcpuregs $7f00

;----------------------------------------------------------------------------
; Single Wire Interface Module

		include	"stm8/swim.inc"
		__defswim $7f80

;----------------------------------------------------------------------------
; Debug Module

		include	"stm8/dm.inc"
		__defdm	$7f90

;----------------------------------------------------------------------------
; AWU

		include	"awu.inc"
		__defawu $50f0

                restore                 ; allow again

                endif			; __stm8s005c6inc
