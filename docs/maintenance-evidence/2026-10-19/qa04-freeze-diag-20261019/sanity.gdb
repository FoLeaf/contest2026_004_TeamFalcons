set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
monitor halt
printf "=== SANITY ===\n"
info registers pc lr sp basepri primask
printf "g_system_ticks=%u\n", *(unsigned int*)0x24007394
printf "g_live_cycle=%u\n", *(unsigned int*)0x24020c1c
printf "g_arm_freeze_diag @0x24003cd0 (20 words)\n"
x/20uw 0x24003cd0
monitor resume
detach
quit
