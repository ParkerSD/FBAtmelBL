/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 * --------------------
 * SAM-BA Implementation on SAMD21 and SAMD51
 * --------------------
 * Requirements to use SAM-BA :
 *
 * Supported communication interfaces (SAMD21):
 * --------------------
 *
 * SERCOM5 : RX:PB23 TX:PB22
 * Baudrate : 115200 8N1
 *
 * USB : D-:PA24 D+:PA25
 *
 * Pins Usage
 * --------------------
 * The following pins are used by the program :
 * PA25 : input/output
 * PA24 : input/output
 * PB23 : input
 * PB22 : output
 * PA15 : input
 *
 * The application board shall avoid driving the PA25,PA24,PB23,PB22 and PA15
 * signals
 * while the boot program is running (after a POR for example)
 *
 * Clock system
 * --------------------
 * CPU clock source (GCLK_GEN_0) - 8MHz internal oscillator (OSC8M)
 * SERCOM5 core GCLK source (GCLK_ID_SERCOM5_CORE) - GCLK_GEN_0 (i.e., OSC8M)
 * GCLK Generator 1 source (GCLK_GEN_1) - 48MHz DFLL in Clock Recovery mode
 * (DFLL48M)
 * USB GCLK source (GCLK_ID_USB) - GCLK_GEN_1 (i.e., DFLL in CRM mode)
 *
 * Memory Mapping
 * --------------------
 * SAM-BA code will be located at 0x0 and executed before any applicative code.
 *
 * Applications compiled to be executed along with the bootloader will start at
 * 0x2000 (samd21) or 0x4000 (samd51)
 *
 */

#include "uf2.h"
#include "atmel_start.h"
#include "driver_init.h"

static void check_start_application(void);

static volatile bool main_b_cdc_enable = false;
extern int8_t led_tick_step;

#if defined(SAMD21)
    #define RESET_CONTROLLER PM
#elif defined(SAMD51)
    #define RESET_CONTROLLER RSTC
#endif

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void) {
    uint32_t app_start_address;

    /* Load the Reset Handler address of the application */
    app_start_address = *(uint32_t *)(APP_START_ADDRESS + 4);

    /*
     * Test reset vector of application @APP_START_ADDRESS+4
     * Sanity check on the Reset_Handler address
     */
    if (app_start_address < APP_START_ADDRESS || app_start_address > FLASH_SIZE) {
        /* Stay in bootloader */
        return;
    }


	/*
    if (RESET_CONTROLLER->RCAUSE.bit.POR)
	{
		//if reason for reset was power-on reset 
    }
	*/
	

	
    if (gpio_get_pin_level(BL_BOOT_PIN)) //check if BL_BOOT_PIN is high
	{
        return; // stay in bootloader
    }
  


    /* Rebase the Stack Pointer */
    __set_MSP(*(uint32_t *)APP_START_ADDRESS);

    /* Rebase the vector table base address */
    SCB->VTOR = ((uint32_t)APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

    /* Jump to application Reset Handler in the application */
    asm("bx %0" ::"r"(app_start_address));
}

extern char _etext;
extern char _end;

/**
 *  \brief  SAM-BA Main loop.
 *  \return Unused (ANSI-C compatibility).
 */
int main(void) 
{
    // if VTOR is set, we're not running in bootloader mode; halt
    if (SCB->VTOR)
        while (1) {
        }

#if defined(SAMD21)
    // If fuses have been reset to all ones, the watchdog ALWAYS-ON is
    // set, so we can't turn off the watchdog.  Set the fuse to a
    // reasonable value and reset. This is a mini version of the fuse
    // reset code in selfmain.c.
    if (((uint32_t *)NVMCTRL_AUX0_ADDRESS)[0] == 0xffffffff) {
        // Clear any error flags.
        NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
        // Turn off cache and put in manual mode.
        NVMCTRL->CTRLB.reg = NVMCTRL->CTRLB.reg | NVMCTRL_CTRLB_CACHEDIS | NVMCTRL_CTRLB_MANW;
        // Set address to write.
        NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS / 2;
        // Erase auxiliary row.
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_EAR;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        // Clear page buffer.
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        // Reasonable fuse values, including 8k BOOTPROT.
        ((uint32_t *)NVMCTRL_AUX0_ADDRESS)[0] = 0xD8E0C7FA;
        ((uint32_t *)NVMCTRL_AUX0_ADDRESS)[1] = 0xFFFFFC5D;
        // Write the fuses
	NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WAP;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        resetIntoBootloader();
    }

    // Disable the watchdog, in case the application set it.
    WDT->CTRL.reg = 0;
    while(WDT->STATUS.bit.SYNCBUSY) {}

#elif defined(SAMD51)
    // Disable the watchdog, in case the application set it.
    WDT->CTRLA.reg = 0;
    while(WDT->SYNCBUSY.reg) {}

    // Enable 2.7V brownout detection. The default fuse value is 1.7
    // Set brownout detection to ~2.7V. Default from factory is 1.7V,
    // which is too low for proper operation of external SPI flash chips (they are 2.7-3.6V).
    // Also without this higher level, the SAMD51 will write zeros to flash intermittently.
    // Disable while changing level.

    SUPC->BOD33.bit.ENABLE = 0;
    while (!SUPC->STATUS.bit.B33SRDY) {}  // Wait for BOD33 to synchronize.
    SUPC->BOD33.bit.LEVEL = 200;  // 2.7V: 1.5V + LEVEL * 6mV.
    // Don't reset right now.
    SUPC->BOD33.bit.ACTION = SUPC_BOD33_ACTION_NONE_Val;
    SUPC->BOD33.bit.ENABLE = 1; // enable brown-out detection

    // Wait for BOD33 peripheral to be ready.
    while (!SUPC->STATUS.bit.BOD33RDY) {}

    // Wait for voltage to rise above BOD33 value.
    while (SUPC->STATUS.bit.BOD33DET) {}

    // If we are starting from a power-on or a brownout,
    // wait for the voltage to stabilize. Don't do this on an
    // external reset because it interferes with the timing of double-click.
    // "BODVDD" means BOD33.
    if (RSTC->RCAUSE.bit.POR || RSTC->RCAUSE.bit.BODVDD) {
        do {
            // Check again in 100ms.
            delay_ms(100);
        } while (SUPC->STATUS.bit.BOD33DET);
    }

    // Now enable reset if voltage falls below minimum.
    SUPC->BOD33.bit.ENABLE = 0;
    while (!SUPC->STATUS.bit.B33SRDY) {}  // Wait for BOD33 to synchronize.
    SUPC->BOD33.bit.ACTION = SUPC_BOD33_ACTION_RESET_Val;
    SUPC->BOD33.bit.ENABLE = 1;
#endif

    //assert((uint32_t)&_etext < APP_START_ADDRESS); // asserts conflicting with utils_assert.h
      // bossac writes at 0x20005000
    //assert(!USE_MONITOR || (uint32_t)&_end < 0x20005000);
    //assert(8 << NVMCTRL->PARAM.bit.PSZ == FLASH_PAGE_SIZE);
    //assert(FLASH_PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP == FLASH_SIZE);
	
	gpio_init(); 
	
    /* Jump in application if condition is satisfied */
    check_start_application();

    /* We have determined we should stay in the monitor. */
    /* System initialization */
	
    //system_init_ada();
	atmel_start_init(); //init new bootloader peripherals instead, is init_mcu() still needed here? 

    __DMB();
    __enable_irq();

}
