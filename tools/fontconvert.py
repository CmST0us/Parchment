#!/usr/bin/env python3
"""
fontconvert.py — Parchment 字体转换工具。

从 TrueType/OpenType 字体生成 epdiy C header 或 .pfnt 二进制文件。
支持 custom/gb2312-1/gbk/gb18030 字符集预设。

用法:
  # C header (向后兼容)
  python fontconvert.py ui_font_20 20 NotoSansCJKsc.otf --compress --charset gb2312-1

  # .pfnt 二进制
  python fontconvert.py noto_cjk_24 24 NotoSansCJKsc.otf --compress --output-format pfnt --charset gb18030
"""

import sys
import os
import struct
import zlib
import math
import argparse
from collections import namedtuple

try:
    import freetype
except ImportError:
    sys.exit(
        "freetype-py is required.\n"
        "Install with: pip install freetype-py"
    )

# ──────────────────────────────────────────────────────────────────────
# 字符集定义
# ──────────────────────────────────────────────────────────────────────

# 通用标点/符号区间（所有预设字符集共用）
_COMMON_INTERVALS = [
    (0x20, 0x7E),       # ASCII printable
    (0xA0, 0xFF),        # Latin-1 supplement
    (0x2010, 0x205F),    # General punctuation
    (0x2190, 0x21FF),    # Arrows
    (0x2300, 0x23FF),    # Misc technical
    (0x25A0, 0x25FF),    # Geometric shapes
    (0x2600, 0x26F0),    # Misc symbols
    (0x2700, 0x27BF),    # Dingbats
    (0x3000, 0x303F),    # CJK symbols and punctuation
    (0x3040, 0x309F),    # Hiragana
    (0x30A0, 0x30FF),    # Katakana
    (0xFE30, 0xFE4F),    # CJK compatibility forms
    (0xFF00, 0xFFEF),    # Halfwidth and fullwidth forms
]


def _decode_charset(encoding, row_ranges):
    """从指定编码的行列范围生成 Unicode 码点列表。"""
    points = set()
    for row_start, row_end, col_start, col_end in row_ranges:
        for row in range(row_start, row_end + 1):
            for col in range(col_start, col_end + 1):
                try:
                    char = bytes([row, col]).decode(encoding)
                    points.add(ord(char))
                except (UnicodeDecodeError, ValueError):
                    continue
    return points


def get_gb2312_level1_points():
    """GB2312 一级汉字 (3,755 字)：行 0xB0-0xD7，列 0xA1-0xFE。"""
    points = set()
    for row in range(0xB0, 0xD8):
        col_end = 0xF9 if row == 0xD7 else 0xFE
        for col in range(0xA1, col_end + 1):
            try:
                char = bytes([row, col]).decode("gb2312")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    return points


def get_gb2312_points():
    """GB2312 一级+二级汉字 (~6,763 字)：行 0xB0-0xF7，列 0xA1-0xFE。"""
    points = set()
    for row in range(0xB0, 0xF8):
        for col in range(0xA1, 0xFF):
            try:
                char = bytes([row, col]).decode("gb2312")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    return points


def get_japanese_points():
    """日语字符：假名 + JIS X 0208 汉字/符号。"""
    points = set()
    # 平假名 U+3040-U+309F
    for cp in range(0x3040, 0x30A0):
        points.add(cp)
    # 片假名 U+30A0-U+30FF
    for cp in range(0x30A0, 0x3100):
        points.add(cp)
    # 片假名语音扩展 U+31F0-U+31FF
    for cp in range(0x31F0, 0x3200):
        points.add(cp)
    # 半角片假名 U+FF65-U+FF9F
    for cp in range(0xFF65, 0xFFA0):
        points.add(cp)
    # CJK 符号和标点 U+3000-U+303F
    for cp in range(0x3000, 0x3040):
        points.add(cp)
    # 全角拉丁/符号 U+FF01-U+FF60
    for cp in range(0xFF01, 0xFF61):
        points.add(cp)
    # JIS X 0208 符号区 (EUC-JP rows 1-8)
    for high in range(0xA1, 0xA9):
        for low in range(0xA1, 0xFF):
            try:
                char = bytes([high, low]).decode("euc_jp")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    # JIS X 0208 第一水准+第二水准汉字 (EUC-JP rows 16-84)
    for high in range(0xB0, 0xF5):
        for low in range(0xA1, 0xFF):
            try:
                char = bytes([high, low]).decode("euc_jp")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    return points


def get_gbk_points():
    """GBK 全部汉字 (~21,886 字)。"""
    points = set()
    # GBK Level 1+2 (GB2312 区域)
    for row in range(0xB0, 0xF8):
        for col in range(0xA1, 0xFF):
            try:
                char = bytes([row, col]).decode("gbk")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    # GBK Extension A
    for row in range(0xA1, 0xA8):
        for col in range(0x40, 0xFF):
            if col == 0x7F:
                continue
            try:
                char = bytes([row, col]).decode("gbk")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    # GBK Extension B
    for row in range(0xA8, 0xFE + 1):
        for col in range(0x40, 0xA1):
            if col == 0x7F:
                continue
            try:
                char = bytes([row, col]).decode("gbk")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    # GBK Level 3+4
    for row in range(0x81, 0xA1):
        for col in range(0x40, 0xFF):
            if col == 0x7F:
                continue
            try:
                char = bytes([row, col]).decode("gbk")
                points.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                continue
    return points


def get_gb18030_points():
    """GB18030 强制集 BMP 范围内全部 CJK 字符。"""
    points = set()
    # 双字节区域（包含 GBK 全部）
    for first in range(0x81, 0xFF):
        for second in range(0x40, 0xFF):
            if second == 0x7F:
                continue
            try:
                char = bytes([first, second]).decode("gb18030")
                cp = ord(char)
                # 只保留 BMP 范围
                if cp <= 0xFFFF:
                    points.add(cp)
            except (UnicodeDecodeError, ValueError):
                continue
    return points


def resolve_charset_intervals(charset_name, string_arg, face):
    """根据 --charset 参数解析出 Unicode interval 列表。"""
    if charset_name == "custom":
        if string_arg is None:
            # 使用默认的 ASCII + 标点
            return _COMMON_INTERVALS[:]
        # 从 --string 参数生成 intervals
        string = " " + string_arg
        chars = sorted(set(string))
        code_points = []
        for char in chars:
            if face.get_char_index(ord(char)) != 0:
                code_points.append(ord(char))
            else:
                print(f"Character '{char}' not in font", file=sys.stderr)
        return _code_points_to_intervals(code_points)

    # 预设字符集：获取码点集合
    print(f"Generating {charset_name} character set...", file=sys.stderr)
    if charset_name == "gb2312-1":
        cjk_points = get_gb2312_level1_points()
    elif charset_name == "gb2312":
        cjk_points = get_gb2312_points()
    elif charset_name == "gbk":
        cjk_points = get_gbk_points()
    elif charset_name == "gb18030":
        cjk_points = get_gb18030_points()
    elif charset_name == "gb2312+ja":
        cjk_points = get_gb2312_points() | get_japanese_points()
    else:
        raise ValueError(f"Unknown charset: {charset_name}")

    # 合并通用区间和 CJK 码点
    common_points = set()
    for start, end in _COMMON_INTERVALS:
        for cp in range(start, end + 1):
            common_points.add(cp)

    all_points = sorted(common_points | cjk_points)
    print(f"  Total unique code points: {len(all_points)}", file=sys.stderr)

    return _code_points_to_intervals(all_points)


def _code_points_to_intervals(code_points):
    """将排序后的码点列表转换为连续 interval 列表。"""
    if not code_points:
        return []
    intervals = []
    start = code_points[0]
    prev = code_points[0]
    for cp in code_points[1:]:
        if cp == prev + 1:
            prev = cp
        else:
            intervals.append((start, prev))
            start = cp
            prev = cp
    intervals.append((start, prev))
    return intervals


# ──────────────────────────────────────────────────────────────────────
# 光栅化
# ──────────────────────────────────────────────────────────────────────

GlyphProps = namedtuple(
    "GlyphProps",
    ["width", "height", "advance_x", "left", "top",
     "compressed_size", "data_offset", "code_point"],
)


def norm_floor(val):
    return int(math.floor(val / (1 << 6)))


def norm_ceil(val):
    return int(math.ceil(val / (1 << 6)))


def rasterize_glyphs(font_stack, intervals, compress):
    """光栅化所有 interval 中的字符，返回 (all_glyphs, metrics)。"""
    total_size = 0
    all_glyphs = []  # [(GlyphProps, compressed_bytes), ...]
    total_chars = 0
    ascender = 0
    descender = 100
    f_height = 0

    for i_start, i_end in intervals:
        for code_point in range(i_start, i_end + 1):
            # 查找字体中的字符
            face = None
            for f in font_stack:
                if f.get_char_index(code_point) != 0:
                    face = f
                    break
            if face is None:
                continue

            face.load_glyph(
                face.get_char_index(code_point), freetype.FT_LOAD_RENDER
            )

            # 更新 metrics
            if ascender < face.size.ascender:
                ascender = face.size.ascender
            if descender > face.size.descender:
                descender = face.size.descender
            if f_height < face.size.height:
                f_height = face.size.height
            total_chars += 1

            bitmap = face.glyph.bitmap
            pixels = []
            px = 0
            for i, v in enumerate(bitmap.buffer):
                x = i % bitmap.width
                if x % 2 == 0:
                    px = v >> 4
                else:
                    px = px | (v & 0xF0)
                    pixels.append(px)
                    px = 0
                if x == bitmap.width - 1 and bitmap.width % 2 > 0:
                    pixels.append(px)
                    px = 0

            packed = bytes(pixels)
            compressed = zlib.compress(packed) if compress else packed

            glyph = GlyphProps(
                width=bitmap.width,
                height=bitmap.rows,
                advance_x=norm_floor(face.glyph.advance.x),
                left=face.glyph.bitmap_left,
                top=face.glyph.bitmap_top,
                compressed_size=len(compressed),
                data_offset=total_size,
                code_point=code_point,
            )
            total_size += len(compressed)
            all_glyphs.append((glyph, compressed))

            # 进度指示（大字符集时有用）
            if total_chars % 5000 == 0:
                print(f"  Rasterized {total_chars} glyphs...",
                      file=sys.stderr)

    metrics = {
        "total_chars": total_chars,
        "ascender": ascender,
        "descender": descender,
        "f_height": f_height,
    }
    return all_glyphs, metrics


# ──────────────────────────────────────────────────────────────────────
# C header 输出 (原有格式)
# ──────────────────────────────────────────────────────────────────────

def output_header(font_name, all_glyphs, intervals, compress, metrics):
    """输出 epdiy C header 格式到 stdout。"""
    glyph_data = []
    glyph_props = []
    for props, compressed in all_glyphs:
        glyph_data.extend(compressed)
        glyph_props.append(props)

    total_chars = metrics["total_chars"]
    f_height = metrics["f_height"]
    ascender = metrics["ascender"]
    descender = metrics["descender"]

    def chunks(lst, n):
        for i in range(0, len(lst), n):
            yield lst[i:i + n]

    print("#pragma once")
    print('#include "epdiy.h"')
    print(f"/* {font_name}: {total_chars} glyphs, "
          f"{len(glyph_data)} bytes bitmap */")

    print(f"const uint8_t {font_name}_Bitmaps[{len(glyph_data)}] = {{")
    for c in chunks(glyph_data, 16):
        print("    " + " ".join(f"0x{b:02X}," for b in c))
    print("};")

    print(f"const EpdGlyph {font_name}_Glyphs[] = {{")
    for g in glyph_props:
        vals = ", ".join(str(a) for a in list(g[:-1]))
        safe_char = chr(g.code_point) if 32 < g.code_point < 127 else f"U+{g.code_point:04X}"
        print(f"    {{ {vals} }}, // '{safe_char}'")
    print("};")

    print(f"const EpdUnicodeInterval {font_name}_Intervals[] = {{")
    offset = 0
    for i_start, i_end in intervals:
        print(f"    {{ 0x{i_start:X}, 0x{i_end:X}, 0x{offset:X} }},")
        offset += i_end - i_start + 1
    print("};")

    print(f"const EpdFont {font_name} = {{")
    print(f"    {font_name}_Bitmaps,")
    print(f"    {font_name}_Glyphs,")
    print(f"    {font_name}_Intervals,")
    print(f"    {len(intervals)},")
    print(f"    {1 if compress else 0},")
    print(f"    {norm_ceil(f_height)},")
    print(f"    {norm_ceil(ascender)},")
    print(f"    {norm_floor(descender)},")
    print("};")


# ──────────────────────────────────────────────────────────────────────
# .pfnt 二进制输出
# ──────────────────────────────────────────────────────────────────────

PFNT_MAGIC = 0x544E4650  # "PFNT" little-endian
PFNT_VERSION = 1
PFNT_FLAG_COMPRESSED = 0x01


def output_pfnt(font_name, size, all_glyphs, intervals, compress, metrics):
    """输出 .pfnt 二进制格式到 stdout (binary mode)。"""
    glyph_props = []
    bitmap_data = bytearray()
    for props, compressed in all_glyphs:
        glyph_props.append(props)
        bitmap_data.extend(compressed)

    interval_count = len(intervals)
    glyph_count = len(glyph_props)
    flags = PFNT_FLAG_COMPRESSED if compress else 0

    f_height = metrics["f_height"]
    ascender = metrics["ascender"]
    descender = metrics["descender"]

    # Header: 32 bytes (ascender/descender are signed i16)
    header = struct.pack(
        "<IBBHHhhII10s",
        PFNT_MAGIC,
        PFNT_VERSION,
        flags,
        size,
        norm_ceil(f_height),         # advance_y (u16)
        norm_ceil(ascender),         # ascender (i16)
        norm_floor(descender),       # descender (i16)
        interval_count,
        glyph_count,
        b"\x00" * 10,
    )
    assert len(header) == 32, f"Header size: {len(header)}"

    # Intervals: 12 bytes each
    intervals_bin = bytearray()
    offset = 0
    for i_start, i_end in intervals:
        intervals_bin.extend(struct.pack("<III", i_start, i_end, offset))
        offset += i_end - i_start + 1

    # Glyph table: 20 bytes each
    glyphs_bin = bytearray()
    for g in glyph_props:
        glyphs_bin.extend(struct.pack(
            "<HHHhhIIH",
            g.width, g.height, g.advance_x,
            g.left, g.top,
            g.compressed_size, g.data_offset,
            0,  # reserved
        ))

    # 写入 stdout (binary)
    out = sys.stdout.buffer
    out.write(header)
    out.write(bytes(intervals_bin))
    out.write(bytes(glyphs_bin))
    out.write(bytes(bitmap_data))

    total_size = len(header) + len(intervals_bin) + len(glyphs_bin) + len(bitmap_data)
    print(f"  {font_name}: {metrics['total_chars']} glyphs, "
          f"{total_size} bytes total, "
          f"{len(bitmap_data)} bytes bitmap",
          file=sys.stderr)


# ──────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Generate epdiy C header or .pfnt binary from a font."
    )
    parser.add_argument("name", help="Output font name identifier.")
    parser.add_argument("size", type=int, help="Font size in pixels.")
    parser.add_argument(
        "fontstack", nargs="+",
        help="Font file(s), ordered by descending priority.",
    )
    parser.add_argument(
        "--compress", action="store_true",
        help="Compress glyph bitmaps with zlib.",
    )
    parser.add_argument(
        "--output-format", dest="output_format",
        choices=["header", "pfnt"], default="header",
        help="Output format: 'header' (C header) or 'pfnt' (binary). Default: header.",
    )
    parser.add_argument(
        "--charset", default="custom",
        choices=["custom", "gb2312-1", "gb2312", "gb2312+ja", "gbk", "gb18030"],
        help="Character set preset. Default: custom (use --string).",
    )
    parser.add_argument(
        "--string", default=None,
        help="Explicit character string (only with --charset custom).",
    )
    parser.add_argument(
        "--additional-intervals", dest="additional_intervals",
        action="append",
        help="Additional code point intervals as min,max.",
    )
    args = parser.parse_args()

    # 加载字体
    font_stack = [freetype.Face(f) for f in args.fontstack]
    for face in font_stack:
        face.set_char_size(args.size << 6, args.size << 6, 72, 72)

    # 解析字符集
    intervals = resolve_charset_intervals(
        args.charset, args.string, font_stack[0]
    )

    # 附加 intervals
    if args.additional_intervals:
        add_ints = [
            tuple(int(n, base=0) for n in i.split(","))
            for i in args.additional_intervals
        ]
        intervals = sorted(intervals + add_ints)

    print(f"Rasterizing {args.name} at {args.size}px...", file=sys.stderr)

    # 光栅化
    all_glyphs, metrics = rasterize_glyphs(
        font_stack, intervals, args.compress
    )

    print(f"  Done: {metrics['total_chars']} glyphs rasterized.",
          file=sys.stderr)

    # 输出
    if args.output_format == "header":
        output_header(
            args.name, all_glyphs, intervals, args.compress, metrics
        )
    elif args.output_format == "pfnt":
        output_pfnt(
            args.name, args.size, all_glyphs, intervals,
            args.compress, metrics
        )


if __name__ == "__main__":
    main()
