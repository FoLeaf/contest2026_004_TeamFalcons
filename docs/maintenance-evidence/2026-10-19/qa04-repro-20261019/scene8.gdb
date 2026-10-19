set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
set $i = 0
set $systick_seen = 0
while $i < 40
  monitor halt
  set $icsr = *(unsigned int*)0xE000ED04
  set $vectactive = $icsr & 0x1ff
  set $vectpending = ($icsr >> 12) & 0x1ff
  printf "S%02d active=%d pending=%d ticks=%08x shpr3=%08x\n", $i, $vectactive, $vectpending, *(unsigned int*)0x24007344, *(unsigned int*)0xE000ED20
  monitor resume
  shell sleep 0.12
  set $i = $i + 1
end
monitor resume
detach
quit
