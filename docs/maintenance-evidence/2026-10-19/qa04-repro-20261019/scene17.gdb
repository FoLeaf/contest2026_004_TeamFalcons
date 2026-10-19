set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "IRQVEC_14_15_38_39\n"
x/8wx 0x24003e40+14*8
printf "IRQVEC_15\n"
x/2wx 0x24003eb8
printf "USART2_ISR_CR1\n"
x/1wx 0x4000441c
x/1wx 0x40004400
printf "USART3_ISR_CR1\n"
x/1wx 0x4000481c
x/1wx 0x40004800
printf "NVIC_ISPEND\n"
x/1wx 0xE000E204
printf "SYST_CSR\n"
x/1wx 0xE000E010
monitor resume
detach
quit
