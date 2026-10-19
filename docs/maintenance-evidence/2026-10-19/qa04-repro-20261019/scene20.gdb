set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "USART2_STATE_T0\n"
x/1wx 0x4000441c
x/1wx 0x40004420
x/1wx 0xE000E204
x/1wx 0x24007344
printf "USART3_STATE_T0\n"
x/1wx 0x4000481c
x/1wx 0xE000E204
printf "DUMP_BUFFERS\n"
x/252bx 0x24003bd4
monitor resume
shell sleep 1
monitor halt
printf "USART2_STATE_T1\n"
x/1wx 0x4000441c
x/1wx 0x40004420
x/1wx 0xE000E204
x/1wx 0x24007344
printf "DUMP_BUFFERS_T1\n"
x/252bx 0x24003bd4
monitor resume
detach
quit
