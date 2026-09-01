# Power-fail config store notes

## Host AC1

```bash
make -C app/velaguard/host_tests clean test
```

`test_config_store: OK` — empty→factory, commit/seq, damage fallback, both-bad factory, CRC corrupt.

## Board (velaguard-emmc)

```bash
bash scripts/build.sh emmc
```

Boot log: `vgcfg: FACTORY|OK seq=… name=…`  
NSH:

```text
vgcfg probe
vgcfg commit mydev
vgcfg dump
# reset
vgcfg dump
vgcfg damage b trunc
vgcfg dump
vgcfg damage a trunc
vgcfg dump          # FACTORY
```

Files: `/mnt/emmc/velaguard/config/point_table_{a,b}.json`

### Board debug (2026-08-29)

- Boot race: wait for `/mnt/emmc` before load (fixed).
- `commit -5 (errno=0)`: explicit `-EIO` (mkdir final check or verify). Switched to POSIX `open/write/fsync`; commit prints `vg_config_last_error()` stage (`mkdir|write0|write1|verify`).
- Use `vgcfg probe` first to isolate FAT mkdir/write/readback.
### Board evidence (2026-08-29)

| AC | 结果 |
|----|------|
| AC2 | 复位后 boot/`dump`：`OK seq=2 name=mydev` |
| AC3 | commit A/B → damage b → `OK seq=3 name=slotA` |
| AC4 | damage a → `both slots invalid` + `FACTORY` |

Host AC1 + notes AC5 同步通过。

## API

`vg_config_set_basedir`, `vg_config_load`, `vg_config_commit`, `vg_config_factory_default`, `vg_config_damage_slot`, `vg_config_probe`, `vg_config_last_error`
