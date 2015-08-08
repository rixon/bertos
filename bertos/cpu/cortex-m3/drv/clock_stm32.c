/**
 * \file
 * <!--
 * This file is part of BeRTOS.
 *
 * Bertos is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 *
 * Copyright 2010 Develer S.r.l. (http://www.develer.com/)
 *
 * -->
 *
 * \brief STM32 Clocking driver.
 *
 * \author Andrea Righi <arighi@develer.com>
 */

#include "clock_stm32.h"

#include <cfg/compiler.h>
#include <cfg/debug.h>

#include <io/stm32.h>

#if CPU_CM3_STM32F1

struct RCC *RCC;

INLINE int rcc_get_flag_status(uint32_t flag)
{
	uint32_t id;
	reg32_t reg;

	/* Get the RCC register index */
	id = flag >> 5;
	/* The flag to check is in CR register */
	if (id == 1)
		reg = RCC->CR;
	/* The flag to check is in BDCR register */
	else if (id == 2)
		reg = RCC->BDCR;
	/* The flag to check is in CSR register */
	else
		reg = RCC->CSR;
	/* Get the flag position */
	id = flag & FLAG_MASK;

	return reg & (1 << id);
}

INLINE uint16_t pll_clock(void)
{
	unsigned int div, mul;

	/* Hopefully this is evaluate at compile time... */
	for (div = 2; div; div--)
		for (mul = 2; mul <= 16; mul++)
			if (CPU_FREQ <= (PLL_VCO / div * mul))
				break;
	return mul << 8 | div;
}

INLINE void rcc_pll_config(void)
{
	reg32_t reg = RCC->CFGR & CFGR_PLL_MASK;

	/* Evaluate clock parameters */
	uint16_t clock = pll_clock();
	uint32_t pll_mul = ((clock >> 8) - 2) << 18;
	uint32_t pll_div = ((clock & 0xff) << 1 | 1) << 16;

	/* Set the PLL configuration bits */
	reg |= pll_div | pll_mul;

	/* Store the new value */
	RCC->CFGR = reg;

	/* Enable PLL */
	*CR_PLLON_BB = 1;
}

INLINE void rcc_set_clock_source(uint32_t source)
{
	reg32_t reg;

	reg = RCC->CFGR & CFGR_SW_MASK;
	reg |= source;
	RCC->CFGR = reg;
}

void clock_init(void)
{
	/* Initialize global RCC structure */
	RCC = (struct RCC *)RCC_BASE;

	/* Enable the internal oscillator */
	*CR_HSION_BB = 1;
	while (!rcc_get_flag_status(RCC_FLAG_HSIRDY));

	/* Clock the system from internal HSI RC (8 MHz) */
	rcc_set_clock_source(RCC_SYSCLK_HSI);

	/* Enable external oscillator */
	RCC->CR &= CR_HSEON_RESET;
	RCC->CR &= CR_HSEBYP_RESET;
	RCC->CR |= CR_HSEON_SET;
	while (!rcc_get_flag_status(RCC_FLAG_HSERDY));

	/* Initialize PLL according to CPU_FREQ */
	rcc_pll_config();
	while(!rcc_get_flag_status(RCC_FLAG_PLLRDY));

	/* Configure USB clock (48MHz) */
	*CFGR_USBPRE_BB = RCC_USBCLK_PLLCLK_1DIV5;
	/* Configure ADC clock: PCLK2 (9MHz) */
	RCC->CFGR &= CFGR_ADCPRE_RESET_MASK;
	RCC->CFGR |= RCC_PCLK2_DIV8;
	/* Configure system clock dividers: PCLK2 (72MHz) */
	RCC->CFGR &= CFGR_PPRE2_RESET_MASK;
	RCC->CFGR |= RCC_HCLK_DIV1 << 3;
	/* Configure system clock dividers: PCLK1 (36MHz) */
	RCC->CFGR &= CFGR_PPRE1_RESET_MASK;
	RCC->CFGR |= RCC_HCLK_DIV2;
	/* Configure system clock dividers: HCLK */
	RCC->CFGR &= CFGR_HPRE_RESET_MASK;
	RCC->CFGR |= RCC_SYSCLK_DIV1;

	/* Set 1 wait state for the flash memory */
	*(reg32_t *)FLASH_BASE = 0x12;

	/* Clock the system from the PLL */
	rcc_set_clock_source(RCC_SYSCLK_PLLCLK);
}

#elif CPU_CM3_STM32L1

/* ===============         Settings  =======================*/

#define STM32_HSI_ENABLED                   1
#define STM32_LSI_ENABLED                   1
#define STM32_HSE_ENABLED                   0
#define STM32_LSE_ENABLED                   0
#define STM32_ADC_CLOCK_ENABLED             1
#define STM32_USB_CLOCK_ENABLED             1
#define STM32_MSIRANGE                      STM32_MSIRANGE_2M
#define STM32_SW                            STM32_SW_PLL
#define STM32_PLLSRC                        STM32_PLLSRC_HSI
#define STM32_PLLMUL_VALUE                  6
#define STM32_PLLDIV_VALUE                  3
#define STM32_HPRE                          STM32_HPRE_DIV1
#define STM32_PPRE1                         STM32_PPRE1_DIV1
#define STM32_PPRE2                         STM32_PPRE2_DIV1
#define STM32_MCOSEL                        STM32_MCOSEL_NOCLOCK
#define STM32_MCOPRE                        STM32_MCOPRE_DIV1
#define STM32_RTCSEL                        STM32_RTCSEL_NOCLOCK
#define STM32_RTCPRE                        STM32_RTCPRE_DIV2
#define STM32_VOS                           STM32_VOS_1P8
#define STM32_PVD_ENABLE                    0
#define STM32_PLS                           STM32_PLS_LEV0
#define STM32_ACTIVATE_PLL                  0

/* ===============       End Settings  =======================*/

/**
 * @name    PWR_CR register bits definitions
 * @{
 */
#define STM32_VOS_MASK          (3 << 11)   /**< Core voltage mask.         */
#define STM32_VOS_1P8           (1 << 11)   /**< Core voltage 1.8 Volts.    */
#define STM32_VOS_1P5           (2 << 11)   /**< Core voltage 1.5 Volts.    */
#define STM32_VOS_1P2           (3 << 11)   /**< Core voltage 1.2 Volts.    */

#define STM32_PLS_MASK          (7 << 5)    /**< PLS bits mask.             */
#define STM32_PLS_LEV0          (0 << 5)    /**< PVD level 0.               */
#define STM32_PLS_LEV1          (1 << 5)    /**< PVD level 1.               */
#define STM32_PLS_LEV2          (2 << 5)    /**< PVD level 2.               */
#define STM32_PLS_LEV3          (3 << 5)    /**< PVD level 3.               */
#define STM32_PLS_LEV4          (4 << 5)    /**< PVD level 4.               */
#define STM32_PLS_LEV5          (5 << 5)    /**< PVD level 5.               */
#define STM32_PLS_LEV6          (6 << 5)    /**< PVD level 6.               */
#define STM32_PLS_LEV7          (7 << 5)    /**< PVD level 7.               */
/** @} */

/* Voltage related limits.*/
#if STM32_VOS == STM32_VOS_1P8
/**
 * @brief   Maximum HSE clock frequency at current voltage setting.
 */
#define STM32_HSECLK_MAX            32000000

/**
 * @brief   Maximum SYSCLK clock frequency at current voltage setting.
 */
#define STM32_SYSCLK_MAX            32000000

/**
 * @brief   Maximum VCO clock frequency at current voltage setting.
 */
#define STM32_PLLVCO_MAX            96000000

/**
 * @brief   Minimum VCO clock frequency at current voltage setting.
 */
#define STM32_PLLVCO_MIN            6000000

/**
 * @brief   Maximum APB1 clock frequency.
 */
#define STM32_PCLK1_MAX             32000000

/**
 * @brief   Maximum APB2 clock frequency.
 */
#define STM32_PCLK2_MAX             32000000

/**
 * @brief   Maximum frequency not requiring a wait state for flash accesses.
 */
#define STM32_0WS_THRESHOLD         16000000

/**
 * @brief   HSI availability at current voltage settings.
 */
#define STM32_HSI_AVAILABLE         1

#elif STM32_VOS == STM32_VOS_1P5
#define STM32_HSECLK_MAX            16000000
#define STM32_SYSCLK_MAX            16000000
#define STM32_PLLVCO_MAX            48000000
#define STM32_PLLVCO_MIN            6000000
#define STM32_PCLK1_MAX             16000000
#define STM32_PCLK2_MAX             16000000
#define STM32_0WS_THRESHOLD         8000000
#define STM32_HSI_AVAILABLE         1
#elif STM32_VOS == STM32_VOS_1P2
#define STM32_HSECLK_MAX            4000000
#define STM32_SYSCLK_MAX            4000000
#define STM32_PLLVCO_MAX            24000000
#define STM32_PLLVCO_MIN            6000000
#define STM32_PCLK1_MAX             4000000
#define STM32_PCLK2_MAX             4000000
#define STM32_0WS_THRESHOLD         2000000
#define STM32_HSI_AVAILABLE         0
#else
#error "invalid STM32_VOS value specified"
#endif

#define PWR ((struct PWR*)PWR_BASE)
#define RCC ((struct RCC*)RCC_BASE)

/**
 * @brief   STM32L1xx voltage, clocks and PLL initialization.
 * @note    All the involved constants come from the file @p board.h.
 * @note    This function should be invoked just after the system reset.
 *
 * @special
 */
/**
 * @brief   Clocks and internal voltage initialization.
 */
void clock_init(void)
{
	/* PWR clock enable.*/
	RCC->APB1ENR = RCC_APB1ENR_PWREN;

	/* Core voltage setup.*/
	while ((PWR->CSR & PWR_CSR_VOSF) != 0)
		;                           /* Waits until regulator is stable.         */
	PWR->CR = STM32_VOS;
	while ((PWR->CSR & PWR_CSR_VOSF) != 0)
		;                           /* Waits until regulator is stable.         */

	/* Initial clocks setup and wait for MSI stabilization, the MSI clock is
	   always enabled because it is the fallback clock when PLL the fails.
	   Trim fields are not altered from reset values.*/
	RCC->CFGR  = 0;
	RCC->ICSCR = (RCC->ICSCR & ~STM32_MSIRANGE_MASK) | STM32_MSIRANGE;
	RCC->CR    = RCC_CR_MSION;
	while ((RCC->CR & RCC_CR_MSIRDY) == 0)
		;                           /* Waits until MSI is stable.               */

#if STM32_HSI_ENABLED
	/* HSI activation.*/
	RCC->CR |= RCC_CR_HSION;
	while ((RCC->CR & RCC_CR_HSIRDY) == 0)
		;                           /* Waits until HSI is stable.               */
#endif

#if STM32_HSE_ENABLED
#if defined(STM32_HSE_BYPASS)
	/* HSE Bypass.*/
	RCC->CR |= RCC_CR_HSEBYP;
#endif
	/* HSE activation.*/
	RCC->CR |= RCC_CR_HSEON;
	while ((RCC->CR & RCC_CR_HSERDY) == 0)
		;                           /* Waits until HSE is stable.               */
#endif

#if STM32_LSI_ENABLED
	/* LSI activation.*/
	RCC->CSR |= RCC_CSR_LSION;
	while ((RCC->CSR & RCC_CSR_LSIRDY) == 0)
		;                           /* Waits until LSI is stable.               */
#endif

#if STM32_LSE_ENABLED
	/* LSE activation, have to unlock the register.*/
	if ((RCC->CSR & RCC_CSR_LSEON) == 0) {
		PWR->CR |= PWR_CR_DBP;
		RCC->CSR |= RCC_CSR_LSEON;
		PWR->CR &= ~PWR_CR_DBP;
	}
	while ((RCC->CSR & RCC_CSR_LSERDY) == 0)
		;                           /* Waits until LSE is stable.               */
#endif

#if STM32_ACTIVATE_PLL
	/* PLL activation.*/
	RCC->CFGR |= STM32_PLLDIV | STM32_PLLMUL | STM32_PLLSRC;
	RCC->CR   |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY))
		;                           /* Waits until PLL is stable.               */
#endif

	/* Other clock-related settings (dividers, MCO etc).*/
	RCC->CR   |= STM32_RTCPRE;
	RCC->CFGR |= STM32_MCOPRE | STM32_MCOSEL |
		STM32_PPRE2 | STM32_PPRE1 | STM32_HPRE;
	RCC->CSR  |= STM32_RTCSEL;

	/* Flash setup and final clock selection.*/
#if defined(STM32_FLASHBITS1)
	FLASH->ACR = STM32_FLASHBITS1;
#endif
#if defined(STM32_FLASHBITS2)
	FLASH->ACR = STM32_FLASHBITS2;
#endif

	/* Switching to the configured clock source if it is different from MSI.*/
#if (STM32_SW != STM32_SW_MSI)
	RCC->CFGR |= STM32_SW;        /* Switches on the selected clock source.   */
	while ((RCC->CFGR & RCC_CFGR_SWS) != (STM32_SW << 2))
		;
#endif

	/* SYSCFG clock enabled here because it is a multi-functional unit shared
	   among multiple drivers.*/
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;                                                   \
	RCC->APB2LPENR |= RCC_APB2ENR_SYSCFGEN;                                               \
}


#else /* CPU_CM3_STM32F2 */

/* PLL_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N */
#define PLL_M      25
#define PLL_N      212

/* SYSCLK = PLL_VCO / PLL_P */
#define PLL_P      2

/* USB OTG FS, SDIO and RNG Clock =  PLL_VCO / PLLQ */
#define PLL_Q      5

/* PLLI2S_VCO = (HSE_VALUE Or HSI_VALUE / PLL_M) * PLLI2S_N
   I2SCLK = PLLI2S_VCO / PLLI2S_R */
#define PLLI2S_N   212
#define PLLI2S_R   5



#define HSE_STARTUP_TIMEOUT    ((uint16_t)0x0500)   /*!< Time out for HSE start up */

void clock_init(void)
{
	/* Reset the RCC clock configuration to the default reset state ------------*/
	/* Set HSION bit */
	RCC->CR |= (uint32_t)0x00000001;

	/* Reset CFGR register */
	RCC->CFGR = 0x00000000;

	/* Reset HSEON, CSSON and PLLON bits */
	RCC->CR &= (uint32_t)0xFEF6FFFF;

	/* Reset PLLCFGR register */
	RCC->PLLCFGR = 0x24003010;

	/* Reset HSEBYP bit */
	RCC->CR &= (uint32_t)0xFFFBFFFF;

	/* Disable all interrupts */
	RCC->CIR = 0x00000000;

	uint32_t StartUpCounter = 0, HSEStatus = 0;

	/* Enable HSE */
	RCC->CR |= ((uint32_t)RCC_CR_HSEON);

	/* Wait till HSE is ready and if Time out is reached exit */
	do
	{
		HSEStatus = RCC->CR & RCC_CR_HSERDY;
		StartUpCounter++;
	} while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

	if ((RCC->CR & RCC_CR_HSERDY) != 0)
	{
		HSEStatus = (uint32_t)0x01;
	}
	else
	{
		HSEStatus = (uint32_t)0x00;
	}

	if (HSEStatus == (uint32_t)0x01)
	{
		/* HCLK = SYSCLK / 1*/
		RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

		/* PCLK2 = HCLK / 2*/
		RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

		/* PCLK1 = HCLK / 4*/
		RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;

		/* Configure the main PLL */
		RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) -1) << 16) |
					   (RCC_PLLCFGR_PLLSRC_HSE) | (PLL_Q << 24);

		/* Enable the main PLL */
		RCC->CR |= RCC_CR_PLLON;

		/* Wait till the main PLL is ready */
		while((RCC->CR & RCC_CR_PLLRDY) == 0)
		{
		}

		/* Configure Flash prefetch, Instruction cache, Data cache and wait state */
		FLASH->ACR = FLASH_ACR_PRFTEN |FLASH_ACR_ICEN |FLASH_ACR_DCEN |FLASH_ACR_LATENCY_3WS;

		/* Select the main PLL as system clock source */
		RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
		RCC->CFGR |= RCC_CFGR_SW_PLL;

		/* Wait till the main PLL is used as system clock source */
		while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS ) != RCC_CFGR_SWS_PLL);
		{
		}
	}
	else
	{ /* If HSE fails to start-up, the application will have wrong clock
		 configuration. User can add here some code to deal with this error */
	}


	/******************************************************************************/
	/*            I2S clock configuration (For devices Rev B and Y)               */
	/******************************************************************************/
	/* PLLI2S clock used as I2S clock source */
	RCC->CFGR &= ~RCC_CFGR_I2SSRC;

	/* Configure PLLI2S */
	RCC->PLLI2SCFGR = (PLLI2S_N << 6) | (PLLI2S_R << 28);

	/* Enable PLLI2S */
	RCC->CR |= ((uint32_t)RCC_CR_PLLI2SON);

	/* Wait till PLLI2S is ready */
	while((RCC->CR & RCC_CR_PLLI2SRDY) == 0)
	{
	}
}

#endif
