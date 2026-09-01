# MThings SYS_DATA Model

## Purpose

Use this reference when generating or modifying pages and widgets inside the `SYS_DATA` section of a `.mthings` project file.

## XML Shape

`SYS_DATA` contains page definitions first, followed by top-level widget definitions:

```xml
<SYS_DATA>
  <PAGE ID="" Name="" width="" hight="" BackColor="" PicPath="">
    <CHIPV ID="" Width="" Height="" PosX="" PosY="" />
  </PAGE>
  <CHIPV ID="" lock="" Type="" Intvl="" ExtroPara="">
    <DeviceData DeviceID="" DataID="" ExtroPara="" />
  </CHIPV>
</SYS_DATA>
```

The nested `PAGE/CHIPV` node is only the widget placement geometry. The top-level `SYS_DATA/CHIPV` node is the actual widget configuration.

## Page Nodes

Each `PAGE` should include:

- `ID`: unique page ID.
- `Name`: display name.
- `width`: page width.
- `hight`: page height. Preserve this spelling.
- `BackColor`: ARGB color string.
- `PicPath`: background image path, or empty.

## Placement Nodes

Each `PAGE/CHIPV` placement should include:

- `ID`: widget ID referencing a top-level `SYS_DATA/CHIPV`.
- `Width` and `Height`: placed widget size.
- `PosX` and `PosY`: top-left position on the page.

A single top-level widget definition may be placed on more than one page by adding multiple `PAGE/CHIPV` geometry nodes with the same `ID`. This is common for page-tab/navigation widgets.

## Widget Definitions

Each top-level `SYS_DATA/CHIPV` should include:

- `ID`: unique widget ID.
- `lock`: `0` for editable widgets unless preserving an existing value.
- `Type`: widget type ID.
- `Intvl`: refresh interval. Use `10` by default.
- `ExtroPara`: widget-level extended parameters in `id::value;id::value;` format.

Bound data points are stored as child `DeviceData` nodes:

```xml
<DeviceData DeviceID="1" DataID="1" ExtroPara="" />
```

Each binding must reference an existing `DEVICE_LIST/DEVICE/DATA_LIST/DATA` pair.

## ID Rules

- Keep page IDs unique.
- Keep top-level widget IDs unique.
- Assign new IDs as max existing numeric ID plus 1 unless the user provides explicit IDs.
- Every `PAGE/CHIPV ID` must have exactly one matching top-level `SYS_DATA/CHIPV ID`.
- Do not duplicate top-level widget definitions to place the same navigation widget on multiple pages.

## Navigation

Navigation widgets store the target page in widget-level `ExtroPara` as `pageid::<ID>;`. The target must be an existing page ID.

## Layout

- Keep `PosX`, `PosY`, `Width`, and `Height` inside the page bounds.
- Page size may be `1920x920` in saved desktop projects rather than `1920x1080`. Generate the size requested by the user; otherwise use `1920x920` when mimicking UI-saved demos and `1920x1080` for full-HD standalone screens.
- Color strings may be lower-case ARGB (`#ffffffff`) or upper-case ARGB (`#FFFFFFFF`). Treat them equivalently and preserve existing casing when patching.

## Validation

- XML parses and `SYS_DATA` exists.
- Every placement references an existing widget definition.
- Every widget binding references an existing device/data point.
- No widget exceeds its catalog max data-binding count.
- Navigation `pageid` values point to existing pages.
- Existing `lock` values are preserved when patching unless the user asks to unlock or lock widgets.
