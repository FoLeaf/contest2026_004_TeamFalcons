set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "STATE3\n"
info registers pc lr sp msp psp basepri primask
printf "ICSR\n"
x/1wx 0xE000ED04
printf "TICKS3\n"
x/1wx 0x24007344
printf "SAMPLE_A\n"
x/1wx 0x24007344
x/1wx 0xE000E018
printf "SLEEP_200MS\n"
shell sleep 0.2
printf "SAMPLE_B\n"
x/1wx 0x24007344
x/1wx 0xE000E018
printf "SLEEP_1S\n"
shell sleep 1
printf "SAMPLE_C\n"
x/1wx 0x24007344
x/1wx 0xE000E018
printf "SAMPLE_PC\n"
info registers pc lr
x/1wx 0xE000ED04
printf "SLEEP_2S\n"
shell sleep 2
printf "SAMPLE_D\n"
x/1wx 0x24007344
x/1wx 0xE000E018
info registers pc lr
x/1wx 0xE000ED04
monitor resume
detach
quit
