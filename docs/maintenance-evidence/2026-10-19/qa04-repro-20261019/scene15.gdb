set pagination off
set confirm off
set height 0
set width 0
file /home/hello19y/openvela/.vg-maintenance-closeout/.debug/nuttx.elf
target remote 127.0.0.1:3333
set $i = 0
while $i < 30
  monitor halt
  set $icsr = *(unsigned int*)0xE000ED04
  printf "S%02d active=%d pending=%d isrpending=%d ticks=%08x\n", $i, $icsr & 0x1ff, ($icsr >> 12) & 0x1ff, ($icsr >> 22) & 1, *(unsigned int*)0x24007344
  monitor resume
  shell sleep 0.07
  set $i = $i + 1
end
monitor resume
detach
quit
