# Widget Generation Reference

## Purpose

Use this reference when choosing widget types, binding data points, sizing widgets, or tuning `ExtroPara` values in `SYS_DATA`.

## ExtroPara Format

Widget-level and per-data extended parameters use:

```text
id::value;id::value;
```

Rules:

- Preserve semicolon-delimited order from existing widgets when patching.
- Use common parameter IDs such as `text`, `tlname`, `uint`, `max`, `min`, `backcolor`, `pageid`, `fsize`, `tlcolor`, and `vlcolor`.
- Use ARGB color strings such as `#FF000000` or transparent `#00000000`.
- Switch-style values are usually `ON` or `OFF`.
- Unknown IDs should be kept unchanged when modifying existing config.

## Catalog Lookup

Run this only when type IDs, default sizes, max bindings, or default parameters are not already known:

```powershell
python mthings-automation-config-skill/scripts/extract_chip_catalog.py .
```

The script returns JSON with:

- widget enum/name and numeric type ID
- max bound data count
- default width and height
- default `ExtroPara` ID/value pairs

Use the JSON as a local catalog for generation. Do not expose catalog-discovery implementation details in generated project explanations.

## Common Intent Mapping

- Numeric display: `CTEN_LABEL`, `CTEN_DATA_PANEL`
- Large metric card: `CTEN_ARITH_SUM`, `CTEN_MULCMD`
- Gauge: `CTEN_GAUGE`, `CTEN_GAUGE_SPEED`
- Percent/progress: `CTEN_PROGR_1`, `CTEN_PROGR_2`, `CTEN_PROGR_3`
- Curve: `CTEN_CURV_R`, `CTEN_CURV_D`, `CTEN_CURV_H`, `CTEN_BAR_CRV`
- Table: `CTEN_TBL`
- Binary state/control: `CTEN_SWITCH`, `CTEN_BTN_SWITCH`, `CTEN_STATE_LIGHT`, `CTEN_PIC_SWITCH`
- Command: `CTEN_CMD_BTN`, `CTEN_CMD_TBL`, `CTEN_MUL_CMD`, `CTEN_RESET_BTN`
- Navigation: `CTEN_PAGE_TAB`, `CTEN_BTN_LINK`
- Decoration/static: `CTEN_TEXT`, `CTEN_SPLIT`, `CTEN_PIC_PANEL`, `CTEN_FLOW_PATH`

## Layout Heuristics

- Use catalog default dimensions as the initial size.
- Place widgets on a simple grid unless the user requests a diagram-like layout.
- Keep `PosX`, `PosY`, `Width`, and `Height` inside the page's `width`/`hight`.
- Use page tabs or link buttons when generating more than one page.
- Use meaningful `tlname`, `text`, and `uint` values from the device data name/unit.
