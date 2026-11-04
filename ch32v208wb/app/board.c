#include "demo.h"
#include "debug.h"
#include "ch32v20x_usb.h"
#include <stdio.h>
#include <string.h>

static uint8_t ep1_dma[64] __attribute__((aligned(4)));
static volatile uint32_t milliseconds;
static char serial_number[24];
static volatile uint32_t setup_count;
static uint8_t last_setup[8];

uint32_t board_millis(void) { return milliseconds; }
uint32_t board_core_clock(void) { return SystemCoreClock; }
const char *board_serial(void) { return serial_number; }
void board_address(uint8_t address) { USBFSD->DEV_ADDR=address; }
void board_ep0_rx(int data1)
{
    USBFSD->UEP0_TX_CTRL=USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL=USBFS_UEP_R_RES_ACK | (data1 ? USBFS_UEP_R_TOG : 0);
}
void board_ep0_stall(void)
{
    USBFSD->UEP0_TX_CTRL=USBFS_UEP_T_RES_STALL;
    USBFSD->UEP0_RX_CTRL=USBFS_UEP_R_RES_STALL;
}
void board_ep0_tx(const uint8_t *data, unsigned length, int data1)
{
    if (length && data!=demo_ep0) memcpy(demo_ep0,data,length);
    USBFSD->UEP0_TX_LEN=length;
    USBFSD->UEP0_TX_CTRL=USBFS_UEP_T_RES_ACK | (data1 ? USBFS_UEP_T_TOG : 0);
}
void board_bulk_stop(void) { USBFSD->UEP2_TX_CTRL=(USBFSD->UEP2_TX_CTRL&~USBFS_UEP_T_RES_MASK)|USBFS_UEP_T_RES_NAK; }
void board_bulk_tx(const uint8_t *data, unsigned length, int data1)
{
    USBFSD->UEP2_DMA=(uint32_t)data;
    USBFSD->UEP2_TX_LEN=length;
    USBFSD->UEP2_TX_CTRL=USBFS_UEP_T_RES_ACK | (data1 ? USBFS_UEP_T_TOG : 0);
}
void board_endpoints(void)
{
    USBFSD->UEP4_1_MOD=USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD=USBFS_UEP2_TX_EN;
    USBFSD->UEP5_6_MOD=0;
    USBFSD->UEP0_DMA=(uint32_t)demo_ep0;
    USBFSD->UEP1_DMA=(uint32_t)ep1_dma;
    USBFSD->UEP0_TX_LEN=USBFSD->UEP1_TX_LEN=USBFSD->UEP2_TX_LEN=0;
    USBFSD->UEP0_TX_CTRL=USBFSD->UEP1_TX_CTRL=USBFSD->UEP2_TX_CTRL=USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL=USBFS_UEP_R_RES_ACK;
}
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USBFS_IRQHandler(void)
{
    uint8_t flags=USBFSD->INT_FG, status=USBFSD->INT_ST;
    unsigned endpoint=status&USBFS_UIS_ENDP_MASK;
    demo_usb_irqs++;
    if (flags&USBFS_UIF_BUS_RST) {
        USBFSD->DEV_ADDR=0; board_endpoints(); demo_usb_reset();
        USBFSD->INT_FG=USBFS_UIF_BUS_RST;
    } else if (flags&USBFS_UIF_TRANSFER) {
        switch (status&USBFS_UIS_TOKEN_MASK) {
        case USBFS_UIS_TOKEN_SETUP:
            /* Like EVT, SETUP always uses EP0. On this board the status endpoint
             * bits can still be nonzero (e.g. 2 after bulk IN); filtering them
             * drops valid control requests and can also block enumeration. */
            setup_count++; memcpy(last_setup,demo_ep0,8);
            USBFSD->UEP0_TX_CTRL=USBFS_UEP_T_RES_NAK;
            USBFSD->UEP0_RX_CTRL=USBFS_UEP_R_RES_NAK;
            demo_usb_setup();
            break;
        case USBFS_UIS_TOKEN_IN:
            if (endpoint==0) USBFSD->UEP0_TX_CTRL=(USBFSD->UEP0_TX_CTRL&~USBFS_UEP_T_RES_MASK)|USBFS_UEP_T_RES_NAK;
            if (endpoint==2) board_bulk_stop();
            demo_usb_in(endpoint);
            break;
        case USBFS_UIS_TOKEN_OUT:
            if (status&USBFS_UIS_TOG_OK) demo_usb_out(endpoint,USBFSD->RX_LEN);
            break;
        }
        USBFSD->INT_FG=USBFS_UIF_TRANSFER;
    } else USBFSD->INT_FG=flags;
}
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    static unsigned microseconds, phase;
    if (TIM_GetITStatus(TIM2,TIM_IT_Update)!=RESET) {
        TIM_ClearITPendingBit(TIM2,TIM_IT_Update);
        microseconds+=63;
        if (microseconds>=1000) { milliseconds++; microseconds-=1000; }
        phase=(phase+1)&63;
        unsigned duty=(demo_brightness+3u)>>2;
        GPIO_WriteBit(GPIOD,GPIO_Pin_2,phase<duty ? Bit_SET : Bit_RESET);
        GPIO_WriteBit(GPIOD,GPIO_Pin_3,(milliseconds/500)&1 ? Bit_SET : Bit_RESET);
    }
}
static void peripherals_init(void)
{
    GPIO_InitTypeDef gpio={0}; USART_InitTypeDef uart={0}; TIM_TimeBaseInitTypeDef timer={0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOD|RCC_APB2Periph_USART1,ENABLE);
    GPIO_ResetBits(GPIOD,GPIO_Pin_2|GPIO_Pin_3);
    gpio.GPIO_Pin=GPIO_Pin_2|GPIO_Pin_3; gpio.GPIO_Mode=GPIO_Mode_Out_PP; gpio.GPIO_Speed=GPIO_Speed_2MHz;
    GPIO_Init(GPIOD,&gpio);
    gpio.GPIO_Pin=GPIO_Pin_9; gpio.GPIO_Mode=GPIO_Mode_AF_PP; gpio.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&gpio);
    gpio.GPIO_Pin=GPIO_Pin_10; gpio.GPIO_Mode=GPIO_Mode_IPU; GPIO_Init(GPIOA,&gpio);
    uart.USART_BaudRate=115200; uart.USART_WordLength=USART_WordLength_8b;
    uart.USART_StopBits=USART_StopBits_1; uart.USART_Parity=USART_Parity_No;
    uart.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
    uart.USART_Mode=USART_Mode_Tx|USART_Mode_Rx; USART_Init(USART1,&uart); USART_Cmd(USART1,ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
    timer.TIM_Prescaler=SystemCoreClock/1000000u-1; timer.TIM_Period=62;
    timer.TIM_CounterMode=TIM_CounterMode_Up; TIM_TimeBaseInit(TIM2,&timer);
    TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE); NVIC_SetPriority(TIM2_IRQn,0x80);
    NVIC_EnableIRQ(TIM2_IRQn); TIM_Cmd(TIM2,ENABLE);
}
int main(void)
{
    SystemCoreClockUpdate(); Delay_Init();
    snprintf(serial_number,sizeof(serial_number),"V208-%08lx",*(volatile uint32_t *)0x1ffff7e8);
    peripherals_init(); demo_init();
    printf("\r\nCH32V208WB USBFS J1, UART PA9/PA10, LED PD2/PD3, core=%lu, BMP=%lu\r\n",
           (unsigned long)SystemCoreClock,(unsigned long)DEMO_FILE_BYTES);
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_Div2);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS,ENABLE);
    USBFSD->BASE_CTRL=USBFS_UC_RESET_SIE|USBFS_UC_CLR_ALL; Delay_Us(10);
    USBFSD->BASE_CTRL=0; USBFSD->INT_FG=0xff;
    USBFSD->INT_EN=USBFS_UIE_BUS_RST|USBFS_UIE_TRANSFER|USBFS_UIE_SUSPEND;
    USBFSD->BASE_CTRL=USBFS_UC_DEV_PU_EN|USBFS_UC_INT_BUSY|USBFS_UC_DMA_EN;
    /* Match EVT: enable DMA/controller before publishing endpoint DMA state. */
    board_endpoints();
    USBFSD->UDEV_CTRL=USBFS_UD_PD_DIS|USBFS_UD_PORT_EN;
    NVIC_EnableIRQ(USBFS_IRQn);
    uint8_t last_configured=0;
    for (;;) {
        demo_poll();
        if (demo_configured!=last_configured) {
            last_configured=demo_configured;
            printf("USB configured=%u at %lu ms\r\n",last_configured,(unsigned long)board_millis());
        }
        if (USART_GetFlagStatus(USART1,USART_FLAG_RXNE)!=RESET) {
            uint8_t c=USART_ReceiveData(USART1);
            if (c=='?') {
                printf("UART RX/TX OK; USB=%u; millis=%lu; LED1=%u; IRQ=%lu SETUP=%lu\r\n",
                       demo_configured,(unsigned long)board_millis(),demo_brightness,
                       (unsigned long)demo_usb_irqs,(unsigned long)setup_count);
                printf("USB ctrl=%02x port=%02x ien=%02x fg=%02x st=%02x addr=%u len=%lu ep0=%02x/%02x\r\n",
                       USBFSD->BASE_CTRL,USBFSD->UDEV_CTRL,USBFSD->INT_EN,USBFSD->INT_FG,
                       USBFSD->INT_ST,USBFSD->DEV_ADDR,(unsigned long)USBFSD->RX_LEN,
                       USBFSD->UEP0_TX_CTRL,USBFSD->UEP0_RX_CTRL);
                printf("last setup=%02x %02x %02x %02x %02x %02x %02x %02x RCC=%08lx/%08lx\r\n",
                       last_setup[0],last_setup[1],last_setup[2],last_setup[3],last_setup[4],
                       last_setup[5],last_setup[6],last_setup[7],
                       (unsigned long)RCC->CFGR0,(unsigned long)RCC->AHBPCENR);
            }
        }
    }
}
