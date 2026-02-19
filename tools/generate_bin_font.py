#!/usr/bin/env python3
"""
generate_bin_font.py — Parchment .bin 字体生成工具。

从 TrueType/OpenType 字体生成自定义 .bin 格式（4bpp 灰度 + RLE 压缩）。
参考 bin-font-format spec: 128B 文件头 + 16B glyph 条目（unicode 升序）+ RLE 位图数据。

用法:
  python generate_bin_font.py LXGWWenKai-Regular.ttf -o reading.bin --charset gb2312 --size 32
  python generate_bin_font.py font.ttf -o ui.bin --range U+0020-U+007E --range U+4E00-U+9FFF --size 32
"""

import sys
import os
import struct
import math
import argparse

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

# GB2312 一级汉字 Unicode 码点范围（粗略映射，通过实际编码表获取更精确）
# 这里使用 GB2312 的实际 Unicode 映射
def _gb2312_level1_codepoints():
    """GB2312 一级汉字（按拼音排序，约 3755 个）+ ASCII + 常用标点。"""
    codepoints = set()
    # ASCII printable
    for cp in range(0x20, 0x7F):
        codepoints.add(cp)
    # CJK 标点和符号
    for cp in range(0x3000, 0x3040):
        codepoints.add(cp)
    # 全角 ASCII
    for cp in range(0xFF00, 0xFF60):
        codepoints.add(cp)
    # General punctuation
    for cp in range(0x2010, 0x2060):
        codepoints.add(cp)
    # GB2312 一级汉字覆盖：CJK Unified Ideographs 常用区
    # 精确的 GB2312 一级需要编码表映射，这里用 GB2312 的实际范围
    # 通过 Python codecs 获取
    import codecs
    for row in range(0xB0, 0xD8):  # 一级汉字区 B0-D7
        for col in range(0xA1, 0xFF):
            try:
                char = bytes([row, col]).decode('gb2312')
                codepoints.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                pass
    return sorted(codepoints)


def _gb18030_codepoints():
    """GB18030 字符集（包含 GB2312 + GBK + 扩展）。"""
    codepoints = set()
    # ASCII
    for cp in range(0x20, 0x7F):
        codepoints.add(cp)
    # CJK 标点
    for cp in range(0x3000, 0x3040):
        codepoints.add(cp)
    for cp in range(0xFF00, 0xFF60):
        codepoints.add(cp)
    for cp in range(0x2010, 0x2060):
        codepoints.add(cp)
    # GB2312 全部（一级 + 二级）
    import codecs
    for row in range(0xB0, 0xF8):
        for col in range(0xA1, 0xFF):
            try:
                char = bytes([row, col]).decode('gb2312')
                codepoints.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                pass
    # GBK 扩展
    for high in range(0x81, 0xFF):
        for low in range(0x40, 0xFF):
            if low == 0x7F:
                continue
            try:
                char = bytes([high, low]).decode('gbk')
                codepoints.add(ord(char))
            except (UnicodeDecodeError, ValueError):
                pass
    return sorted(codepoints)


CHARSET_PRESETS = {
    'gb2312': _gb2312_level1_codepoints,
    'gb18030': _gb18030_codepoints,
}


def parse_range(range_str):
    """解析 U+XXXX-U+YYYY 格式的 Unicode 范围。"""
    parts = range_str.split('-')
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(
            f"Invalid range format: {range_str}. Expected U+XXXX-U+YYYY"
        )
    try:
        start = int(parts[0].replace('U+', '').replace('u+', ''), 16)
        end = int(parts[1].replace('U+', '').replace('u+', ''), 16)
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"Invalid hex values in range: {range_str}"
        )
    return list(range(start, end + 1))


# ──────────────────────────────────────────────────────────────────────
# FreeType 光栅化
# ──────────────────────────────────────────────────────────────────────

def rasterize_glyph(face, codepoint):
    """光栅化单个字符为 8bpp bitmap，返回 glyph 信息或 None（不支持）。
    注意: 调用前须先 face.set_pixel_sizes()。"""
    glyph_index = face.get_char_index(codepoint)
    if glyph_index == 0:
        return None

    face.load_glyph(glyph_index, freetype.FT_LOAD_RENDER)

    bitmap = face.glyph.bitmap
    width = bitmap.width
    height = bitmap.rows
    advance_x = face.glyph.advance.x >> 6  # 26.6 fixed point → pixels
    x_offset = face.glyph.bitmap_left
    y_offset = face.glyph.bitmap_top

    # 8bpp bitmap data — 按行提取（pitch 可能 > width）
    if width > 0 and height > 0:
        buf = bitmap.buffer
        pitch = bitmap.pitch
        if pitch == width:
            pixels_8bpp = bytes(buf[:width * height])
        else:
            pixels_8bpp = b''.join(
                bytes(buf[row * pitch:row * pitch + width])
                for row in range(height)
            )
    else:
        pixels_8bpp = b''

    return {
        'unicode': codepoint,
        'width': width,
        'height': height,
        'advance_x': advance_x,
        'x_offset': x_offset,
        'y_offset': y_offset,
        'pixels_8bpp': pixels_8bpp,
    }


def quantize_to_4bpp(pixels_8bpp):
    """将 8bpp 灰度量化为 4bpp (0-15)。"""
    # 预计算查找表，避免逐像素除法
    return bytes(_QUANTIZE_LUT[p] for p in pixels_8bpp)


# 8bpp → 4bpp 量化查找表
_QUANTIZE_LUT = [round(p / 17.0) for p in range(256)]


# ──────────────────────────────────────────────────────────────────────
# RLE 编码器
# ──────────────────────────────────────────────────────────────────────

def rle_encode_glyph(pixels_4bpp, width, height):
    """
    RLE 编码 4bpp glyph bitmap。
    格式: 高 4 位 = 重复次数 (1-15), 低 4 位 = 灰度值
    每行末尾: 0x00 行结束标记
    """
    encoded = bytearray()

    for row in range(height):
        row_start = row * width
        col = 0
        while col < width:
            gray = pixels_4bpp[row_start + col]
            count = 1
            while (col + count < width and
                   pixels_4bpp[row_start + col + count] == gray and
                   count < 15):
                count += 1
            encoded.append((count << 4) | (gray & 0x0F))
            col += count
        # 行结束标记
        encoded.append(0x00)

    return bytes(encoded)


# ──────────────────────────────────────────────────────────────────────
# .bin 文件输出
# ──────────────────────────────────────────────────────────────────────

HEADER_SIZE = 128
GLYPH_ENTRY_SIZE = 16
MAGIC = b'PFNT'
VERSION = 1


def build_bin_file(glyphs, face, size_px):
    """
    构建 .bin 文件内容。
    返回 bytes。
    """
    # 按 unicode 升序排序
    glyphs.sort(key=lambda g: g['unicode'])
    glyph_count = len(glyphs)

    # 获取字体族名
    family_name = face.family_name
    if isinstance(family_name, bytes):
        family_name = family_name.decode('utf-8', errors='replace')
    family_bytes = family_name.encode('utf-8')[:64]
    family_bytes = family_bytes.ljust(64, b'\x00')

    # ascender/descender
    face.set_pixel_sizes(0, size_px)
    ascender = face.size.ascender >> 6
    descender = abs(face.size.descender >> 6)

    # 文件头 (128 bytes)
    header = bytearray(HEADER_SIZE)
    struct.pack_into('<4s', header, 0, MAGIC)           # magic
    struct.pack_into('<B', header, 4, VERSION)           # version
    struct.pack_into('<B', header, 5, size_px)           # font_height
    struct.pack_into('<I', header, 6, glyph_count)       # glyph_count
    header[10:74] = family_bytes                         # family_name
    struct.pack_into('<H', header, 74, ascender)         # ascender
    struct.pack_into('<H', header, 76, descender)        # descender
    # reserved [78:128] already zero

    # RLE 编码所有 glyph bitmap
    rle_data_list = []
    for g in glyphs:
        if g['width'] > 0 and g['height'] > 0:
            pixels_4bpp = quantize_to_4bpp(g['pixels_8bpp'])
            rle = rle_encode_glyph(pixels_4bpp, g['width'], g['height'])
        else:
            rle = b''
        rle_data_list.append(rle)

    # 计算位图数据起始偏移
    bitmap_base = HEADER_SIZE + glyph_count * GLYPH_ENTRY_SIZE

    # 构建 glyph 条目和计算偏移
    entries = bytearray()
    current_offset = bitmap_base
    raw_total = 0

    for i, g in enumerate(glyphs):
        rle = rle_data_list[i]
        raw_size = g['width'] * g['height']  # 未压缩大小 (4bpp 每像素 4bit, 但这里算像素数)
        raw_total += raw_size

        # x_offset 和 y_offset 是 int8
        x_off = max(-128, min(127, g['x_offset']))
        y_off = max(-128, min(127, g['y_offset']))

        # bitmap_offset: 低 24 位 (3 bytes, little-endian)
        offset_low = current_offset & 0xFFFFFF

        entry = struct.pack('<I B B B b b',
                            g['unicode'],
                            g['width'],
                            g['height'],
                            g['advance_x'],
                            x_off,
                            y_off)
        # bitmap_offset: 3 bytes little-endian
        entry += struct.pack('<I', offset_low)[:3]
        # bitmap_size: 4 bytes
        entry += struct.pack('<I', len(rle))

        entries += entry
        current_offset += len(rle)

    # 合并 RLE 位图数据
    bitmap_data = b''.join(rle_data_list)

    return bytes(header) + bytes(entries) + bitmap_data, raw_total


# ──────────────────────────────────────────────────────────────────────
# 主流程
# ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='Parchment .bin 字体生成工具 (4bpp 灰度 + RLE 压缩)')
    parser.add_argument('input', help='输入 TTF/OTF 字体文件')
    parser.add_argument('-o', '--output', required=True, help='输出 .bin 文件路径')
    parser.add_argument('--size', type=int, default=32, help='基础字号 (默认 32px)')
    parser.add_argument('--charset', action='append', default=[],
                        choices=list(CHARSET_PRESETS.keys()),
                        help='字符集预设 (可多次指定)')
    parser.add_argument('--range', action='append', default=[], dest='ranges',
                        help='Unicode 范围 (格式: U+XXXX-U+YYYY, 可多次指定)')
    args = parser.parse_args()

    # 加载字体
    if not os.path.exists(args.input):
        sys.exit(f"Error: Font file not found: {args.input}")

    face = freetype.Face(args.input)
    family = face.family_name
    if isinstance(family, bytes):
        family = family.decode('utf-8', errors='replace')
    print(f"Loaded font: {family}")
    print(f"Base size: {args.size}px")

    # 收集目标码点
    codepoints = set()
    for charset_name in args.charset:
        generator = CHARSET_PRESETS[charset_name]
        cps = generator()
        codepoints.update(cps)
        print(f"Charset '{charset_name}': {len(cps)} codepoints")

    for range_str in args.ranges:
        cps = parse_range(range_str)
        codepoints.update(cps)
        print(f"Range '{range_str}': {len(cps)} codepoints")

    if not codepoints:
        sys.exit("Error: No codepoints specified. Use --charset or --range.")

    codepoints = sorted(codepoints)
    print(f"Total unique codepoints to process: {len(codepoints)}")

    # 光栅化
    face.set_pixel_sizes(0, args.size)
    glyphs = []
    skipped = 0
    for i, cp in enumerate(codepoints):
        if (i + 1) % 1000 == 0:
            print(f"  Rasterizing... {i + 1}/{len(codepoints)}", flush=True)
        result = rasterize_glyph(face, cp)
        if result is None:
            skipped += 1
            if cp > 0x7F:  # 只对非 ASCII 字符输出警告（减少噪音）
                pass  # 静默跳过，最后汇总
            continue
        glyphs.append(result)

    print(f"\nRasterized: {len(glyphs)} glyphs, skipped: {skipped} unsupported")

    if not glyphs:
        sys.exit("Error: No glyphs rasterized.")

    # 构建 .bin 文件
    bin_data, raw_total = build_bin_file(glyphs, face, args.size)

    # 写入文件
    with open(args.output, 'wb') as f:
        f.write(bin_data)

    # 统计输出
    file_size = len(bin_data)
    header_size = HEADER_SIZE
    entries_size = len(glyphs) * GLYPH_ENTRY_SIZE
    bitmap_size = file_size - header_size - entries_size
    # raw_total 是 4bpp 像素数，实际未压缩字节 = raw_total / 2 (4bpp packed)
    raw_bytes = raw_total / 2.0
    compression_ratio = raw_bytes / bitmap_size if bitmap_size > 0 else 0

    print(f"\n=== Generation Complete ===")
    print(f"  Glyph count: {len(glyphs)}")
    print(f"  File size: {file_size:,} bytes ({file_size / 1024 / 1024:.2f} MB)")
    print(f"    Header: {header_size} bytes")
    print(f"    Entries: {entries_size:,} bytes")
    print(f"    Bitmap (RLE): {bitmap_size:,} bytes")
    print(f"  Compression ratio: {compression_ratio:.2f}:1")
    print(f"  Skipped characters: {skipped}")
    print(f"  Output: {args.output}")


if __name__ == '__main__':
    main()
