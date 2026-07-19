#!/usr/bin/env python3
"""Generate LVGL A8 C descriptors from res/*.svg icons."""

from __future__ import annotations

import ctypes
import ctypes.util
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
RES = REPO / "res"
OUT = REPO / "app/velaguard_app/src/assets"

ICONS = (
    ("setting.svg", "vg_img_setting", 24),
    ("settings_ethernet.svg", "vg_img_ethernet", 24),
    ("WIFI.svg", "vg_img_wifi", 24),
)


class GError(ctypes.Structure):
    _fields_ = [
        ("domain", ctypes.c_uint32),
        ("code", ctypes.c_int),
        ("message", ctypes.c_char_p),
    ]


class RsvgRectangle(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_double),
        ("y", ctypes.c_double),
        ("width", ctypes.c_double),
        ("height", ctypes.c_double),
    ]


def load_libs():
    cairo = ctypes.CDLL(ctypes.util.find_library("cairo"))
    rsvg = ctypes.CDLL(ctypes.util.find_library("rsvg-2"))
    gobject = ctypes.CDLL(ctypes.util.find_library("gobject-2.0"))

    cairo.cairo_image_surface_create.restype = ctypes.c_void_p
    cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    cairo.cairo_create.restype = ctypes.c_void_p
    cairo.cairo_create.argtypes = [ctypes.c_void_p]
    cairo.cairo_set_source_rgba.argtypes = [
        ctypes.c_void_p,
        ctypes.c_double,
        ctypes.c_double,
        ctypes.c_double,
        ctypes.c_double,
    ]
    cairo.cairo_paint.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_flush.argtypes = [ctypes.c_void_p]
    cairo.cairo_image_surface_get_data.restype = ctypes.POINTER(ctypes.c_ubyte)
    cairo.cairo_image_surface_get_data.argtypes = [ctypes.c_void_p]
    cairo.cairo_image_surface_get_stride.restype = ctypes.c_int
    cairo.cairo_image_surface_get_stride.argtypes = [ctypes.c_void_p]
    cairo.cairo_destroy.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_destroy.argtypes = [ctypes.c_void_p]

    rsvg.rsvg_handle_new_from_data.restype = ctypes.c_void_p
    rsvg.rsvg_handle_new_from_data.argtypes = [
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.POINTER(GError)),
    ]
    rsvg.rsvg_handle_render_document.restype = ctypes.c_bool
    rsvg.rsvg_handle_render_document.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.POINTER(RsvgRectangle),
        ctypes.POINTER(ctypes.POINTER(GError)),
    ]
    gobject.g_object_unref.argtypes = [ctypes.c_void_p]
    return cairo, rsvg, gobject


def render_a8(cairo, rsvg, gobject, svg_path: Path, size: int) -> bytes:
    data = svg_path.read_bytes()
    err = ctypes.POINTER(GError)()
    handle = rsvg.rsvg_handle_new_from_data(data, len(data), ctypes.byref(err))
    if not handle:
        msg = err.contents.message.decode() if err else "unknown"
        raise RuntimeError(f"rsvg load failed for {svg_path}: {msg}")

    surface = cairo.cairo_image_surface_create(0, size, size)
    cr = cairo.cairo_create(surface)
    cairo.cairo_set_source_rgba(cr, 0, 0, 0, 0)
    cairo.cairo_paint(cr)
    viewport = RsvgRectangle(0, 0, float(size), float(size))
    err2 = ctypes.POINTER(GError)()
    ok = rsvg.rsvg_handle_render_document(
        handle, cr, ctypes.byref(viewport), ctypes.byref(err2)
    )
    if not ok:
        msg = err2.contents.message.decode() if err2 else "unknown"
        raise RuntimeError(f"render failed {svg_path}: {msg}")

    cairo.cairo_surface_flush(surface)
    stride = cairo.cairo_image_surface_get_stride(surface)
    ptr = cairo.cairo_image_surface_get_data(surface)
    a8 = bytearray(size * size)
    for y in range(size):
        for x in range(size):
            i = y * stride + x * 4
            b, g, r, a = ptr[i], ptr[i + 1], ptr[i + 2], ptr[i + 3]
            a8[y * size + x] = max(a, r, g, b) if a else max(r, g, b)

    cairo.cairo_destroy(cr)
    cairo.cairo_surface_destroy(surface)
    gobject.g_object_unref(handle)
    return bytes(a8)


def emit_c(name: str, size: int, a8: bytes) -> str:
    lines = [
        "/****************************************************************************",
        f" * Generated A8 icon: {name} ({size}x{size})",
        " * Regenerate: python3 scripts/gen_vg_icons.py",
        " ****************************************************************************/",
        "",
        "#include <lvgl/lvgl.h>",
        "",
        f"#ifndef LV_ATTRIBUTE_IMAGE_{name.upper()}",
        f"#define LV_ATTRIBUTE_IMAGE_{name.upper()}",
        "#endif",
        "",
        f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST "
        f"LV_ATTRIBUTE_IMAGE_{name.upper()} uint8_t {name}_map[] = {{",
    ]
    for i in range(0, len(a8), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in a8[i : i + 16])
        lines.append(f"  {chunk},")
    lines.extend(
        [
            "};",
            "",
            f"const lv_image_dsc_t {name} = {{",
            "  .header = {",
            "    .magic = LV_IMAGE_HEADER_MAGIC,",
            "    .cf = LV_COLOR_FORMAT_A8,",
            "    .flags = 0,",
            f"    .w = {size},",
            f"    .h = {size},",
            f"    .stride = {size},",
            "    .reserved_2 = 0,",
            "  },",
            f"  .data_size = sizeof({name}_map),",
            f"  .data = {name}_map,",
            "  .reserved = NULL,",
            "  .reserved_2 = NULL,",
            "};",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    cairo, rsvg, gobject = load_libs()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "vg_icons.h").write_text(
        """/****************************************************************************
 * app/velaguard_app/src/assets/vg_icons.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef APP_VELAGUARD_APP_SRC_ASSETS_VG_ICONS_H
#define APP_VELAGUARD_APP_SRC_ASSETS_VG_ICONS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern const lv_image_dsc_t vg_img_setting;
extern const lv_image_dsc_t vg_img_ethernet;
extern const lv_image_dsc_t vg_img_wifi;

#ifdef __cplusplus
}
#endif

#endif /* APP_VELAGUARD_APP_SRC_ASSETS_VG_ICONS_H */
""",
        encoding="utf-8",
    )

    for svg_name, c_name, size in ICONS:
        a8 = render_a8(cairo, rsvg, gobject, RES / svg_name, size)
        (OUT / f"{c_name}.c").write_text(emit_c(c_name, size, a8), encoding="utf-8")
        print(f"{svg_name}: nonzero={sum(1 for b in a8 if b)}/{len(a8)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
