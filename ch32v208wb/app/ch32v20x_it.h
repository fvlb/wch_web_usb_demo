#ifndef V208_DEMO_INTERRUPTS_H
#define V208_DEMO_INTERRUPTS_H
/* Interrupt implementations are in board.c; startup supplies weak fault traps. */
void USBFS_IRQHandler(void);
void TIM2_IRQHandler(void);
#endif
