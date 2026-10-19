set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "USART2_RECV\n"
x/4wx 0x24000198+0x68
printf "USART2_XMIT\n"
x/4wx 0x24000198+0x4c
printf "USART3_RECV\n"
x/4wx 0x240000cc+0x68
printf "USART3_XMIT\n"
x/4wx 0x240000cc+0x4c
printf "USART2_HW\n"
x/1wx 0x40004400
x/1wx 0x40004408
x/1wx 0x4000441c
printf "USART3_HW\n"
x/1wx 0x40004800
x/1wx 0x40004808
x/1wx 0x4000481c
printf "ISPEND_IRQ38_39_61\n"
x/1wx 0xE000E204
printf "IABR\n"
x/2wx 0xE000E304
monitor resume
detach
quit
