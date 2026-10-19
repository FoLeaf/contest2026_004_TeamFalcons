set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "USART2_UART_DEV\n"
x/24wx 0x24000198
printf "USART2_RXBUF_HEAD_TAIL\n"
printf "priv+? recv struct: size, head, tail\n"
x/8wx 0x24000198+24
printf "USART2_CR1_CR3_ISR\n"
x/1wx 0x40004400
x/1wx 0x40004408
x/1wx 0x4000441c
printf "USART2_ICR_READBACK\n"
x/1wx 0x40004420
printf "NVIC_ISPEND_IRQ38_39\n"
x/2wx 0xE000E200+4
printf "USART3_UART_DEV\n"
x/24wx 0x240000cc
printf "TICKS\n"
x/1wx 0x24007344
monitor resume
detach
quit
