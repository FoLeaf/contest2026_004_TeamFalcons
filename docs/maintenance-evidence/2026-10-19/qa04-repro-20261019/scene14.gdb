set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "USART2_RECV_FULL\n"
x/8wx 0x24000200
printf "USART3_RECV_FULL\n"
x/8wx 0x24000134
printf "USART2_HW\n"
x/1wx 0x40004400
x/1wx 0x4000441c
printf "USART3_HW\n"
x/1wx 0x40004800
x/1wx 0x4000481c
monitor resume
detach
quit
