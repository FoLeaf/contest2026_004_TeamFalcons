set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "USART2_RECV size/head/tail/buffer\n"
x/4wx 0x24000198+0x68+0x14
x/4wx 0x24000198+0x68+0x10
x/4wx 0x24000198+0x68+0x18
printf "USART2_XMIT size/head/tail/buffer\n"
x/4wx 0x24000198+0x4c+0x14
x/4wx 0x24000198+0x4c+0x10
x/4wx 0x24000198+0x4c+0x18
printf "USART3_RECV size/head/tail/buffer\n"
x/4wx 0x240000cc+0x68+0x14
x/4wx 0x240000cc+0x68+0x10
x/4wx 0x240000cc+0x68+0x18
printf "USART3_XMIT size/head/tail/buffer\n"
x/4wx 0x240000cc+0x4c+0x14
x/4wx 0x240000cc+0x4c+0x10
x/4wx 0x240000cc+0x4c+0x18
printf "USART2_HW\n"
x/1wx 0x40004400
x/1wx 0x4000441c
printf "USART3_HW\n"
x/1wx 0x40004800
x/1wx 0x4000481c
printf "NVIC_ISPEND_ACTIVE\n"
x/2wx 0xE000E204
x/2wx 0xE000E304
printf "TICKS\n"
x/1wx 0x24007344
monitor resume
detach
quit
