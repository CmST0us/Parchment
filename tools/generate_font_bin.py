#!/usr/bin/env python3
"""
generate_font_bin.py - Parchment 字体生成工具

将 TTF/OTF 字体预渲染为 1-bit packed bitmap 的 .bin 文件，
供 Parchment 阅读器固件运行时加载。

文件格式兼容 M5ReadPaper V2。

用法:
    python generate_font_bin.py <font_file> <output.bin> [--size N] [--white N]
                                [--no-smooth] [--ascii-only]

依赖:
    pip install freetype-py numpy

可选依赖:
    pip install Pillow   (用于 --demo 预览)

改造自 M5ReadPaper 项目 (https://github.com/shinemoon/M5ReadPaper)
"""

import argparse
import struct
import sys
from pathlib import Path

import numpy as np

try:
    import freetype
except ImportError:
    print("Error: freetype-py is required. Install with: pip install freetype-py")
    sys.exit(1)


# .bin 文件格式常量
BIN_HEADER_SIZE = 134
BIN_GLYPH_ENTRY_SIZE = 20
BIN_VERSION = 2


def build_charset(ascii_only=False):
    """构建目标字符集。

    Args:
        ascii_only: 仅包含 ASCII 字符。

    Returns:
        排序后的 Unicode codepoint 列表。
    """
    chars = set()

    # ASCII 可打印字符 (U+0020 ~ U+007E)
    for cp in range(0x0020, 0x007F):
        chars.add(cp)

    if not ascii_only:
        # CJK 统一汉字基本区 (U+4E00 ~ U+9FFF)
        for cp in range(0x4E00, 0xA000):
            chars.add(cp)

        # 中文标点符号
        cjk_punctuation = (
            # CJK 标点 (U+3000 ~ U+303F) 常用部分
            list(range(0x3000, 0x3004))   # 空格、逗号等
            + list(range(0x3008, 0x3012)) # 括号
            + [0x3014, 0x3015]            # 龟壳括号
            + [0x3016, 0x3017]            # 白角括号
            + [0x301C, 0x301E]            # 波浪号
            # 全角标点 (从 FF00 区)
            + [0xFF01]         # ！
            + [0xFF08, 0xFF09] # （）
            + [0xFF0C]         # ，
            + [0xFF0E]         # ．
            + [0xFF1A, 0xFF1B] # ：；
            + [0xFF1F]         # ？
            + [0xFF5B, 0xFF5D] # ｛｝
            # 常用中文标点
            + [0x2014]         # —（破折号）
            + [0x2018, 0x2019] # ''（单引号）
            + [0x201C, 0x201D] # ""（双引号）
            + [0x2026]         # …（省略号）
            + [0x2030]         # ‰
            + [0x3001, 0x3002] # 、。
        )
        for cp in cjk_punctuation:
            chars.add(cp)

    return sorted(chars)


def smooth_edges(bitmap, width, height):
    """对 glyph bitmap 进行边缘平滑处理。

    检测边缘像素并应用轻度模糊，减少 1-bit 量化锯齿。

    Args:
        bitmap: numpy 2D array, uint8 灰度值。
        width: 图像宽度。
        height: 图像高度。

    Returns:
        平滑后的 bitmap。
    """
    if width < 3 or height < 3:
        return bitmap

    smoothed = bitmap.copy().astype(np.float32)

    # 计算局部方差来检测边缘
    padded = np.pad(smoothed, 1, mode='edge')

    for dy in range(-1, 2):
        for dx in range(-1, 2):
            if dy == 0 and dx == 0:
                continue

    # 简单 3x3 均值滤波，仅作用于边缘区域
    kernel_sum = np.zeros_like(smoothed)
    count = np.zeros_like(smoothed)

    for dy in range(-1, 2):
        for dx in range(-1, 2):
            shifted = padded[1 + dy:1 + dy + height, 1 + dx:1 + dx + width]
            kernel_sum += shifted
            count += 1

    mean = kernel_sum / count
    variance = np.zeros_like(smoothed)
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            shifted = padded[1 + dy:1 + dy + height, 1 + dx:1 + dx + width]
            variance += (shifted - mean) ** 2
    variance /= count

    # 边缘掩码：方差大的区域
    edge_threshold = 1000.0
    edge_mask = variance > edge_threshold

    # 对边缘像素应用均值
    result = bitmap.copy().astype(np.float32)
    result[edge_mask] = mean[edge_mask]

    return np.clip(result, 0, 255).astype(np.uint8)


def render_glyph(face, codepoint, white_threshold, do_smooth):
    """渲染单个字符的 glyph。

    Args:
        face: FreeType face 对象。
        codepoint: Unicode 码位。
        white_threshold: 白色阈值 (0-255)。
        do_smooth: 是否做边缘平滑。

    Returns:
        dict 包含 glyph 元数据和 1-bit packed bitmap，
        或 None 如果字符不存在。
    """
    glyph_index = face.get_char_index(codepoint)
    if glyph_index == 0:
        return None

    face.load_char(codepoint, freetype.FT_LOAD_RENDER)
    glyph = face.glyph
    bitmap = glyph.bitmap

    if bitmap.width == 0 or bitmap.rows == 0:
        # 无 bitmap（如空格）
        return {
            'unicode': codepoint,
            'advance_w': glyph.advance.x >> 6,
            'bitmap_w': 0,
            'bitmap_h': 0,
            'x_offset': 0,
            'y_offset': 0,
            'bitmap_data': b'',
        }

    # 转为 numpy array
    buf = np.array(bitmap.buffer, dtype=np.uint8).reshape(bitmap.rows, bitmap.width)

    # 边缘平滑
    if do_smooth and bitmap.width >= 3 and bitmap.rows >= 3:
        buf = smooth_edges(buf, bitmap.width, bitmap.rows)

    # 二值化：灰度 >= (255 - white_threshold) → 前景(1)，否则 → 背景(0)
    threshold = 255 - white_threshold
    binary = (buf >= threshold).astype(np.uint8)

    # 裁剪空白边缘
    rows_any = np.any(binary, axis=1)
    cols_any = np.any(binary, axis=0)

    if not np.any(rows_any):
        return {
            'unicode': codepoint,
            'advance_w': glyph.advance.x >> 6,
            'bitmap_w': 0,
            'bitmap_h': 0,
            'x_offset': 0,
            'y_offset': 0,
            'bitmap_data': b'',
        }

    row_min = np.argmax(rows_any)
    row_max = len(rows_any) - np.argmax(rows_any[::-1])
    col_min = np.argmax(cols_any)
    col_max = len(cols_any) - np.argmax(cols_any[::-1])

    cropped = binary[row_min:row_max, col_min:col_max]
    crop_h, crop_w = cropped.shape

    # Pack 为 1-bit: MSB first，每行 ceil(w/8) 字节
    row_bytes = (crop_w + 7) // 8
    packed = bytearray()

    for row in range(crop_h):
        for byte_idx in range(row_bytes):
            byte_val = 0
            for bit in range(8):
                px = byte_idx * 8 + bit
                if px < crop_w and cropped[row, px]:
                    byte_val |= (0x80 >> bit)
            packed.append(byte_val)

    # 计算 offset 调整（裁剪导致的偏移）
    x_offset = glyph.bitmap_left + col_min
    y_offset = -(glyph.bitmap_top - row_min)  # bitmap_top 是 baseline 上方距离

    return {
        'unicode': codepoint,
        'advance_w': glyph.advance.x >> 6,
        'bitmap_w': crop_w,
        'bitmap_h': crop_h,
        'x_offset': x_offset,
        'y_offset': y_offset,
        'bitmap_data': bytes(packed),
    }


def get_font_names(face):
    """从 FreeType face 获取字体名称。

    Returns:
        (family_name, style_name) 元组。
    """
    family = face.family_name.decode('utf-8', errors='replace') if face.family_name else 'Unknown'
    style = face.style_name.decode('utf-8', errors='replace') if face.style_name else 'Regular'
    return family, style


def write_bin_file(output_path, font_height, glyphs, family_name, style_name):
    """将 glyph 数据写入 .bin 文件。

    Args:
        output_path: 输出文件路径。
        font_height: 字体高度（像素）。
        glyphs: glyph 数据列表。
        family_name: 字体族名。
        style_name: 字体样式名。
    """
    char_count = len(glyphs)

    # === Header (134 bytes) ===
    header = bytearray(BIN_HEADER_SIZE)
    # char_count (uint32 LE)
    struct.pack_into('<I', header, 0, char_count)
    # font_height (uint8)
    header[4] = font_height
    # version (uint8)
    header[5] = BIN_VERSION
    # family_name (64 bytes, UTF-8)
    family_bytes = family_name.encode('utf-8')[:63]
    header[6:6 + len(family_bytes)] = family_bytes
    # style_name (64 bytes, UTF-8)
    style_bytes = style_name.encode('utf-8')[:63]
    header[70:70 + len(style_bytes)] = style_bytes

    # === Glyph Table ===
    # 先计算 bitmap 数据的起始偏移
    bitmap_start = BIN_HEADER_SIZE + char_count * BIN_GLYPH_ENTRY_SIZE

    glyph_table = bytearray()
    bitmap_data = bytearray()
    bitmap_offset = bitmap_start

    for g in glyphs:
        entry = bytearray(BIN_GLYPH_ENTRY_SIZE)
        struct.pack_into('<H', entry, 0, g['unicode'])
        struct.pack_into('<H', entry, 2, g['advance_w'])
        entry[4] = g['bitmap_w']
        entry[5] = g['bitmap_h']
        entry[6] = g['x_offset'] & 0xFF  # int8 → uint8
        entry[7] = g['y_offset'] & 0xFF
        struct.pack_into('<I', entry, 8, bitmap_offset)
        struct.pack_into('<I', entry, 12, len(g['bitmap_data']))
        # entry[16:20] reserved = 0

        glyph_table.extend(entry)
        bitmap_data.extend(g['bitmap_data'])
        bitmap_offset += len(g['bitmap_data'])

    # === 写入文件 ===
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(glyph_table)
        f.write(bitmap_data)

    total_size = len(header) + len(glyph_table) + len(bitmap_data)
    print(f"  Written: {output_path}")
    print(f"  Characters: {char_count}")
    print(f"  File size: {total_size:,} bytes ({total_size / 1024 / 1024:.1f} MB)")


def main():
    parser = argparse.ArgumentParser(
        description='Parchment 字体生成工具：TTF/OTF → .bin (1-bit bitmap)')
    parser.add_argument('font_file', help='输入 TTF/OTF 字体文件')
    parser.add_argument('output', help='输出 .bin 文件路径')
    parser.add_argument('--size', type=int, default=24,
                        help='字体大小（像素），默认 24')
    parser.add_argument('--white', type=int, default=80,
                        help='白色阈值 (0-255)，越低笔画越细，默认 80')
    parser.add_argument('--no-smooth', action='store_true',
                        help='禁用边缘平滑')
    parser.add_argument('--ascii-only', action='store_true',
                        help='仅生成 ASCII 字符')
    parser.add_argument('--face-index', type=int, default=0,
                        help='TTC 字体集合中的 face 索引，默认 0')
    parser.add_argument('--demo', metavar='TEXT',
                        help='生成预览图（需要 Pillow）')
    parser.add_argument('--demo-out', metavar='PATH', default='demo.png',
                        help='预览图输出路径，默认 demo.png')
    parser.add_argument('--demo-scale', type=int, default=2,
                        help='预览图放大倍数，默认 2')

    args = parser.parse_args()

    # 验证输入
    font_path = Path(args.font_file)
    if not font_path.exists():
        print(f"Error: Font file not found: {font_path}")
        sys.exit(1)

    if args.size < 8 or args.size > 72:
        print(f"Error: Font size must be 8-72, got {args.size}")
        sys.exit(1)

    # 加载字体
    print(f"Loading font: {font_path}")
    face = freetype.Face(str(font_path), index=args.face_index)
    face.set_pixel_sizes(0, args.size)

    family_name, style_name = get_font_names(face)
    print(f"  Font: {family_name} {style_name}")
    print(f"  Size: {args.size}px")

    # 构建字符集
    charset = build_charset(ascii_only=args.ascii_only)
    print(f"  Charset: {len(charset)} characters")

    # 渲染所有 glyph
    do_smooth = not args.no_smooth
    glyphs = []
    skipped = 0

    for i, cp in enumerate(charset):
        if (i + 1) % 1000 == 0 or i == len(charset) - 1:
            print(f"  Rendering: {i + 1}/{len(charset)}", end='\r')

        result = render_glyph(face, cp, args.white, do_smooth)
        if result:
            glyphs.append(result)
        else:
            skipped += 1

    print()
    if skipped > 0:
        print(f"  Skipped {skipped} missing glyphs")

    # 按 unicode 排序
    glyphs.sort(key=lambda g: g['unicode'])

    # 写入 .bin 文件
    print(f"\nWriting {args.output}...")
    write_bin_file(args.output, args.size, glyphs, family_name, style_name)

    # 可选预览
    if args.demo:
        try:
            from PIL import Image, ImageDraw
        except ImportError:
            print("\nWarning: Pillow not installed, skipping demo. Install with: pip install Pillow")
            return

        print(f"\nGenerating demo: {args.demo_out}")
        scale = args.demo_scale
        # 简单渲染预览文本
        demo_w = 600
        demo_h = args.size * 3
        img = Image.new('L', (demo_w, demo_h), 255)

        cursor_x = 4
        cursor_y = 4
        for ch in args.demo:
            cp = ord(ch)
            # 查找 glyph
            g = None
            for gl in glyphs:
                if gl['unicode'] == cp:
                    g = gl
                    break
            if not g:
                cursor_x += args.size // 2
                continue

            if g['bitmap_w'] > 0 and g['bitmap_h'] > 0:
                row_bytes = (g['bitmap_w'] + 7) // 8
                for py in range(g['bitmap_h']):
                    for px in range(g['bitmap_w']):
                        byte_idx = py * row_bytes + px // 8
                        if byte_idx < len(g['bitmap_data']):
                            if g['bitmap_data'][byte_idx] & (0x80 >> (px % 8)):
                                dx = cursor_x + g['x_offset'] + px
                                dy = cursor_y + g['y_offset'] + py
                                if 0 <= dx < demo_w and 0 <= dy < demo_h:
                                    img.putpixel((dx, dy), 0)

            cursor_x += g['advance_w']

        if scale > 1:
            img = img.resize((demo_w * scale, demo_h * scale), Image.NEAREST)

        img.save(args.demo_out)
        print(f"  Saved: {args.demo_out}")

    print("\nDone!")


if __name__ == '__main__':
    main()
