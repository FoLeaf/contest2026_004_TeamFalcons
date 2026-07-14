# Design: mock acquisition seam

## API

```c
enum vg_acq_backend { VG_ACQ_NONE, VG_ACQ_MOCK, VG_ACQ_UART /* later */ };
enum vg_acq_quality { VG_ACQ_Q_NONE, VG_ACQ_Q_GOOD, VG_ACQ_Q_BAD };

struct vg_acq_point {
  const char *name;
  const char *unit;
  float value;
  enum vg_acq_quality quality;
  uint32_t age_ms;
};

int vg_acq_init(void);
void vg_acq_poll(void);           /* advance mock / poll uart later */
const struct vg_acq_point *vg_acq_primary(void);
const char *vg_acq_backend_name(void);
```

## Mock behavior

- Single point `Mock Temperature`, unit `C`.
- Base 45.0 C + slow triangle/sine between ~40–85 so UI and later alarm demos work.
- Update every 1000 ms wall via UI timer calling `vg_acq_poll` then refresh labels.
- Quality always GOOD when mock enabled.

## UI

- Keep primary panel layout; replace static ACQUISITION strings with labels updated each second.
- Show backend tag e.g. `MOCK` in muted text.

## Kconfig

- `VG_ACQ_BACKEND`: choice mock / none (uart reserved optional string later).
- Default: mock when `VG_BUILD_MODE=0` (test), none when production optional — simplest: default mock always for now so board demos work; production can set none until real sensor.

## Files

- `src/vg_acq.h`, `src/vg_acq.c` (dispatch + mock)
- `vg_ui_home.c` refresh acquisition labels
- `Kconfig`, `CMakeLists.txt`, `Makefile` / `Make.defs` as needed
- `app/velaguard_app/README.md` one paragraph

## Non-goals

- Thread for acquisition; UI timer is enough for mock.
- Full sensor registry JSON.
