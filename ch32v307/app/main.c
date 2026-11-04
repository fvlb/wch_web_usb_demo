/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : WinUSB / WebUSB LED + file-transfer demo, CH32V307
 *                       port of ../ch585m/app/main.c. Same descriptors,
 *                       vendor protocol and zero-copy EP2 bulk path on the
 *                       USBHS high-speed peripheral. LED pin is a #define
 *                       in led_pwm.c (board LED not identified yet).
 *******************************************************************************/

#include "ch32v30x.h"
#include "debug.h"
#include "uart_dma.h"
#include "ch32v30x_usbhs_device.h"
#include "usb_desc.h"
#include "led_pwm.h"
#include "version.h"
#include "systick.h"
#include "sched.h"
#include "button.h"
#include "filexfer.h"

/* ---- CPU utilization measurement (user's WFI + TMR3 design) ----
 * TMR3 counts at 1 us (144 MHz / 144). The main loop enters WFI;
 * when an interrupt wakes the CPU, we measure how long we stay
 * awake (ISR + main-loop work) before sleeping again. Accumulated
 * awake-vs-elapsed gives true CPU utilization. */
#include "ch32v30x_tim.h"

#define PROF_TMRTIM     TIM3
#define PROF_TMRRCC     RCC_APB1Periph_TIM3

static volatile uint32_t prof_busy_us  = 0;  /* time awake (ISR + main) */
volatile uint32_t prof_isr_us   = 0;  /* time in USB ISR only */
volatile uint32_t prof_isr_cnt  = 0;  /* USB ISR invocation count */

/* called from USBHS_IRQHandler entry/exit */
volatile uint32_t g_isrEntryTick = 0;

static void Prof_Init(void)
{
    TIM_TimeBaseInitTypeDef t = {0};
    RCC_APB1PeriphClockCmd(PROF_TMRRCC, ENABLE);
    t.TIM_Prescaler = SystemCoreClock / 1000000u - 1;
    t.TIM_Period = 0xFFFF;             /* free-running 16-bit (wraps 65.5 ms) */
    t.TIM_ClockDivision = TIM_CKD_DIV1;
    t.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(PROF_TMRTIM, &t);
    TIM_Cmd(PROF_TMRTIM, ENABLE);
}

static inline uint32_t Prof_Now(void)
{
    return PROF_TMRTIM->CNT;  /* 16-bit; deltas are small */
}

/* Diagnostic vendor request 0x0C, 5x uint32:
 * [0]=EP2 completions [1]=USB IRQs [2]=profiled ISR us [3]=core clock Hz
 * [4]=timer capture. This candidate replaces the old idle/busy layout. */
static uint8_t s_profBuf[20];
const uint8_t *Prof_GetData(void)
{
    extern volatile uint32_t g_ep2Count;
    /* Same first two diagnostic fields as CH585: completion IRQ counts.
     * Duration is measured only when USBHS_PROFILE_IRQ=1. */
    uint32_t vals[5] = {g_ep2Count, prof_isr_cnt, prof_isr_us, SystemCoreClock, Prof_Now()};
    memcpy(s_profBuf, vals, 20);
    return s_profBuf;
}

int main(void)
{
    /* SystemInit() ran from startup. Use SystemCoreClock: the current board
     * configuration selects 96 MHz, not the old comment's assumed 144 MHz. */

    USART_Printf_Init(115200);
    UartDma_Init();   /* switch printf to non-blocking DMA TX */
    printf("\nWinUSB/WebUSB LED demo (CH32V307) v%u.%u.%u\n",
           FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    Prof_Init();
    LED_PWM_Init();
    SysTick_InitMillis();
    sched_add("ledblink", LED_PWM_HeartbeatTick, 50, 1);

    USBD_Device_Init();

    Button_Init();
    FileXfer_Init();

    printf("main loop: WFI + TMR3 profiling\r\n");

    while (1)
    {
        __asm volatile("nop"); /* WFI disabled for debug */
        uint32_t t_wake  = Prof_Now();    /* includes ISR time */
        uint32_t t_done;

        sched_update(SysTick_Millis());
        FileXfer_Pump();
        /* any deferred work done; measure awake time */

        t_done = Prof_Now();
        prof_busy_us += t_done - t_wake;  /* main-loop spin time */
    }
}
