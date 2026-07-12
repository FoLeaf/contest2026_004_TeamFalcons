# VelaGuard ISSUE1 UI Design

## Design Read

This is a fixed 480x272 industrial device home screen for on-site operators.
It uses a trust-first monitoring-console language built directly with LVGL.
It is not a marketing landing page, browser dashboard, or developer console.

- `DESIGN_VARIANCE: 4`
- `MOTION_INTENSITY: 2`
- `VISUAL_DENSITY: 7`

## Hierarchy

The screen prioritizes the Local Safety Loop even before later issues make it
active. Acquisition and Alarm own the larger left region. Network, Audio, and
Time occupy a narrower supporting region because cloud and media capability
must not visually outrank local operation.

The identity band keeps Device ID, storage health, boot ID, and uptime visible
without turning the product surface into a debug terminal. Full logs and
addresses remain on NSH and in GDB.

## Layout

```text
+----------------------------------------------------------+
| VelaGuard                         TEST      FW 0.1.0      |
+----------------------------------------------------------+
| LOCAL GATEWAY                                            |
| vg-test-001                           Storage: READY      |
| Boot 7F2A91C4                         Uptime 00:00:12      |
+------------------------------------+---------------------+
| ACQUISITION                        | NETWORK             |
| Not configured                     | Offline             |
|                                    +---------------------+
| ALARM                              | AUDIO               |
| Not armed                          | Unavailable         |
|                                    +---------------------+
|                                    | TIME                |
|                                    | Unsynced            |
+------------------------------------+---------------------+
```

The composition deliberately rejects the historical row of five equal cards.
That pattern made all subsystems look equally important and reduced readable
space on the physical display.

## Tokens

| Role | Hex | Use |
|---|---|---|
| background | `#0C1218` | screen canvas |
| surface | `#151E26` | main operational regions |
| raised surface | `#1C2731` | header and identity band |
| primary text | `#E7EDF2` | identity and states |
| muted text | `#8D9AA5` | labels and metadata |
| accent | `#39B6B2` | VelaGuard brand and selected emphasis |
| alarm | `#E05B5B` | actual alarm semantics only |
| warning | `#D7A84A` | degraded/warning semantics only |

All panels use an 8 px radius. There are no shadows, gradients, glows,
decorative dots, or mixed shape systems.

## Typography and Copy

Use enabled Montserrat sizes with a clear 20/16/12/10 hierarchy. English copy
is required for ISSUE1 because the current embedded font set lacks Chinese
glyph coverage.

Placeholder copy must be truthful:

- Acquisition: `Not configured`
- Alarm: `Not armed`
- Network: `Offline`
- Audio: `Unavailable`
- Time: `Unsynced`
- Storage: `READY` only after directories and both startup records succeed;
  otherwise `DEGRADED`

## Interaction and Motion

ISSUE1 has no product action to trigger, so the home screen has no fake buttons
or navigation. A one-second uptime refresh communicates liveness. No automatic
decorative animation is allowed. Touch may initialize for platform continuity,
but it has no ISSUE1 control surface.

## Embedded Taste Pre-flight

- One fixed dark theme and one cyan product accent.
- Semantic colors only represent real state.
- No equal five-card row or generic web component pattern.
- No clipped text, wrapped mode/version label, or hidden Device ID.
- No decorative dot, pill overlay, glow, gradient, or fake precision.
- Every visible string is grammatical and maps to an implemented state.
- Storage degradation remains visible without blocking the rescue console.
- Physical display contrast and 480x272 edge spacing are readable.
