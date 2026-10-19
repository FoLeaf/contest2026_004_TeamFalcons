set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "STATE2\n"
info registers pc lr sp msp psp basepri
printf "ICSR\n"
x/1wx 0xE000ED04
printf "TICKS2\n"
x/1wx 0x24007344
printf "SYST_CVR\n"
x/1wx 0xE000E018
printf "USART2\n"
x/9wx 0x40004400
printf "USART3\n"
x/9wx 0x40004800
printf "USART2_PRIV\n"
x/16wx 0x24000198
printf "USART3_PRIV\n"
x/16wx 0x240000cc
printf "STACK_TOP_REGION\n"
x/48wx 0x24047f80
monitor resume
detach
quit
