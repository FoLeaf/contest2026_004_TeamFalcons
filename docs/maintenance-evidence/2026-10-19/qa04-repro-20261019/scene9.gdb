set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "PRIO_ALL_IRQ32_95\n"
x/16wx 0xE000E400+32
printf "SHPR\n"
x/3wx 0xE000ED18
printf "AIRCR\n"
x/1wx 0xE000ED0C
printf "USART2_REGS\n"
x/9wx 0x40004400
printf "USART3_REGS\n"
x/9wx 0x40004800
printf "CR1_DECODE\n"
printf "usart2 cr1=%08x cr3=%08x isr=%08x icr=%08x\n", *(unsigned int*)0x40004400, *(unsigned int*)0x40004408, *(unsigned int*)0x4000441c, *(unsigned int*)0x40004420
monitor resume
detach
quit
