#!/usr/bin/env python3
"""将 SVG 图标转换为 4bpp C 数组头文件。

用法:
    python3 convert_icons.py [--size 32] [--src src/] [--out ../../components/ui_core/ui_icon_data.h]

依赖:
    pip install cairosvg Pillow
"""

import argparse
import os
import sys
from pathlib import Path

try:
    import cairosvg
    from PIL import Image
    import io
except ImportError:
    print("Error: 需要安装依赖: pip install cairosvg Pillow", file=sys.stderr)
    sys.exit(1)


def svg_to_grayscale(svg_path: Path, size: int) -> Image.Image:
    """将 SVG 渲染为指定尺寸的灰度图像。"""
    png_data = cairosvg.svg2png(
        url=str(svg_path),
        output_width=size,
        output_height=size,
    )
    img = Image.open(io.BytesIO(png_data)).convert("RGBA")

    # 白色背景上合成 alpha，然后提取灰度
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    composite = Image.alpha_composite(bg, img)
    gray = composite.convert("L")

    return gray


def grayscale_to_alpha(gray_img: Image.Image) -> list[int]:
    """将灰度图转换为 4bpp alpha 值列表。

    白色(255) → alpha=0 (透明)
    黑色(0)   → alpha=15 (不透明)
    """
    pixels = list(gray_img.getdata())
    alpha_values = []
    for p in pixels:
        # 反转: 黑色=不透明, 白色=透明
        alpha_float = (255 - p) / 255.0
        alpha_4bit = round(alpha_float * 15)
        alpha_4bit = max(0, min(15, alpha_4bit))
        alpha_values.append(alpha_4bit)
    return alpha_values


def pack_4bpp(alpha_values: list[int]) -> bytes:
    """将 4bpp alpha 值列表打包为字节数组。

    每字节存储 2 像素: 低 4 位 = 左像素(偶数), 高 4 位 = 右像素(奇数)。
    与 epdiy framebuffer 格式一致。
    """
    result = bytearray()
    for i in range(0, len(alpha_values), 2):
        low = alpha_values[i]  # 偶数像素 → 低 4 位
        high = alpha_values[i + 1] if i + 1 < len(alpha_values) else 0  # 奇数像素 → 高 4 位
        result.append((high << 4) | low)
    return bytes(result)


def icon_var_name(svg_name: str) -> str:
    """从 SVG 文件名生成 C 变量名。

    例: "arrow-left" → "icon_arrow_left_data"
    """
    base = svg_name.replace("-", "_")
    return f"icon_{base}_data"


def generate_header(icons: list[tuple[str, bytes]], size: int) -> str:
    """生成包含所有图标数据的 C 头文件。"""
    lines = [
        "/**",
        " * @file ui_icon_data.h",
        " * @brief 自动生成的图标位图数据。",
        " *",
        f" * 由 tools/icons/convert_icons.py 生成。",
        f" * 图标尺寸: {size}x{size}, 4bpp alpha。",
        " * 图标源: Tabler Icons (MIT License)。",
        " *",
        " * 请勿手动编辑此文件。",
        " */",
        "",
        "#ifndef UI_ICON_DATA_H",
        "#define UI_ICON_DATA_H",
        "",
        "#include <stdint.h>",
        "",
    ]

    for name, data in icons:
        var = icon_var_name(name)
        lines.append(f"/** {name} 图标 ({size}x{size}, {len(data)} bytes)。 */")
        lines.append(f"static const uint8_t {var}[] = {{")

        # 每行 16 字节
        for row_start in range(0, len(data), 16):
            row = data[row_start : row_start + 16]
            hex_vals = ", ".join(f"0x{b:02X}" for b in row)
            comma = "," if row_start + 16 < len(data) else ""
            lines.append(f"    {hex_vals}{comma}")

        lines.append("};")
        lines.append("")

    lines.append("#endif /* UI_ICON_DATA_H */")
    lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="SVG → 4bpp C array 转换器")
    parser.add_argument("--size", type=int, default=32, help="输出图标尺寸 (默认 32)")
    parser.add_argument("--src", type=str, default="src/", help="SVG 源文件目录")
    parser.add_argument(
        "--out",
        type=str,
        default="../../components/ui_core/ui_icon_data.h",
        help="输出头文件路径",
    )
    args = parser.parse_args()

    src_dir = Path(args.src)
    if not src_dir.is_dir():
        print(f"Error: 源目录不存在: {src_dir}", file=sys.stderr)
        sys.exit(1)

    svg_files = sorted(src_dir.glob("*.svg"))
    if not svg_files:
        print(f"Error: {src_dir} 中没有 SVG 文件", file=sys.stderr)
        sys.exit(1)

    icons = []
    for svg_path in svg_files:
        name = svg_path.stem
        print(f"  转换 {name}.svg → {args.size}x{args.size} 4bpp ...", end=" ")

        gray_img = svg_to_grayscale(svg_path, args.size)
        alpha_values = grayscale_to_alpha(gray_img)
        packed = pack_4bpp(alpha_values)

        expected_size = args.size * args.size // 2
        assert len(packed) == expected_size, (
            f"数据大小错误: 期望 {expected_size}, 实际 {len(packed)}"
        )

        icons.append((name, packed))
        print("OK")

    header = generate_header(icons, args.size)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(header, encoding="utf-8")

    print(f"\n生成 {out_path} ({len(icons)} 个图标, "
          f"共 {sum(len(d) for _, d in icons)} bytes)")


if __name__ == "__main__":
    main()
