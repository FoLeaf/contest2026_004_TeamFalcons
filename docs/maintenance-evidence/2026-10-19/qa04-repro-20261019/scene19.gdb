set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "REGISTER_PC_RAW\n"
info registers pc sp lr msp psp
printf "ICSR\n"
x/1wx 0xE000ED04
printf "MSP_CONTENT\n"
x/32wx $msp
printf "PSP_CONTENT\n"
x/16wx $psp
printf "FAULT\n"
x/1wx 0xE000ED28
x/1wx 0xE000ED2C
monitor resume
detach
quit
