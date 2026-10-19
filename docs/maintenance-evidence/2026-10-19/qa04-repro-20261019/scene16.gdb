set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "SYSTICK_CSR RVR CVR CALIB\n"
x/4wx 0xE000E010
printf "SHPR3\n"
x/1wx 0xE000ED20
printf "ICSR\n"
x/1wx 0xE000ED04
printf "SHCSR\n"
x/1wx 0xE000ED24
printf "AIRCR PRIGROUP\n"
x/1wx 0xE000ED0C
printf "USART2_ISR\n"
x/1wx 0x4000441c
printf "TICKS\n"
x/1wx 0x24007344
monitor resume
shell sleep 0.5
monitor halt
printf "AFTER_0.5S\n"
x/4wx 0xE000E010
x/1wx 0xE000ED04
x/1wx 0x24007344
monitor resume
detach
quit
