# VelaGuard dual-slot config store (FAT)

## Scope / Trigger

Persistent config on eMMC VFAT under `app/velaguard/` (`vg_config_store.*`, NSH `vgcfg`). Use when adding fields, changing JSON shape, or debugging board commit/load failures.

## Signatures

```c
void vg_config_set_basedir(const char *dir);
int  vg_config_load(struct vg_config *out);     /* 0=ok, 1=factory, <0=error */
int  vg_config_commit(const struct vg_config *in);
void vg_config_factory_default(struct vg_config *out);
int  vg_config_damage_slot(int slot, int mode); /* bring-up only */
int  vg_config_probe(char *msg, size_t msglen);
const char *vg_config_last_error(void);
```

Files: `point_table_a.json` / `point_table_b.json` under `CONFIG_VG_CONFIG_BASEDIR` (default `/data/velaguard/config`).

## Contracts

JSON one-liner fields: `schema_version`, `seq`, `committed`, `crc32`, `device_name`.  
CRC32 over canonical payload string `"%u|%u|%d|%s"` (schema|seq|committed|name) — **not** over the crc field itself.  
Boot: wait until `/data` (symlink to eMMC) exists before first `vg_config_load`.  
Commit: write inactive slot twice (committed=0 then 1) via POSIX `open/write/fsync/close`, then read-back verify. Prefer highest valid `seq`.

## Validation & Error Matrix

| Condition | Result |
|-----------|--------|
| Both slots missing/bad CRC/uncommitted | load → factory + log `both slots invalid` |
| One slot valid | load that slot |
| Parse / CRC fail on verify after write | commit → `-EIO`, `last_error` stage `verify` |
| basedir missing | mkdir -p; fail if path is a regular file |

## Good / Base / Bad

- Good: commit → reset → boot `OK seq=N name=…`
- Base: damage inactive/active one side → other seq still loads
- Bad: rely on `sscanf("%[…]")` without `CONFIG_LIBC_SCANSET` (NuttX often off) → verify `-EINVAL`

## Tests Required

- Host: `make -C app/velaguard/host_tests test` (`test_config_store`)
- Board: `vgcfg probe` / `commit` / `dump` / `damage` per task AC2–AC4

## Wrong vs Correct

- Wrong: `rename(.tmp→final)` as sole atomicity on NuttX FAT; or parse `device_name` with scanset when `LIBC_SCANSET=n`.
- Correct: full-file rewrite of inactive slot + fsync; parse with `sscanf` numerics + `strstr` for name; keep handbook field name `seq` (not `ver` — distinct from `schema_version`).
