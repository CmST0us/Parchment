#!/usr/bin/env python3
"""
generate_gbk_table.py
从 Python 内置 codecs 生成 GBK→Unicode 映射表 C 数组。

输出: components/text_encoding/gbk_table.c
映射表: const uint16_t gbk_to_unicode[126][190]
  - 行索引: 首字节 - 0x81 (0x81-0xFE → 0-125, 共 126)
  - 列索引: 次字节 - 0x40 (0x40-0xFE 跳过 0x7F → 0-189, 共 190)
  - 值: Unicode codepoint (uint16_t), 无效映射为 0
"""

import os
import sys


def gbk_col(lo):
    """次字节 → 列索引 (0x40-0x7E → 0-62, 0x80-0xFE → 63-189)。"""
    return (lo - 0x40) if lo < 0x80 else (lo - 0x41)


def generate_table():
    """生成 GBK→Unicode 映射表。"""
    table = [[0] * 190 for _ in range(126)]

    for hi in range(0x81, 0xFF):  # 首字节 0x81-0xFE
        for lo in range(0x40, 0xFF):  # 次字节 0x40-0xFE
            if lo == 0x7F:
                continue  # GBK 跳过 0x7F
            col = gbk_col(lo)
            row = hi - 0x81
            try:
                char = bytes([hi, lo]).decode("gbk")
                cp = ord(char)
                if cp <= 0xFFFF:
                    table[row][col] = cp
            except (UnicodeDecodeError, OverflowError):
                pass  # 无效映射，保持 0

    return table


def write_c_file(table, output_path):
    """将映射表写入 C 源文件。"""
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("/**\n")
        f.write(" * @file gbk_table.c\n")
        f.write(" * @brief GBK→Unicode 映射表（由 generate_gbk_table.py 自动生成）。\n")
        f.write(" *\n")
        f.write(" * 行索引: 首字节 - 0x81 (共 126)\n")
        f.write(" * 列索引: 次字节 < 0x80 ? (次字节 - 0x40) : (次字节 - 0x41) (共 190, 跳过 0x7F)\n")
        f.write(" * 值: Unicode codepoint, 无效映射为 0\n")
        f.write(" */\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("const uint16_t gbk_to_unicode[126][190] = {\n")

        for row_idx, row in enumerate(table):
            hi = row_idx + 0x81
            f.write(f"    /* 0x{hi:02X} */ {{\n")
            # 每行 16 个值
            for i in range(0, 190, 16):
                chunk = row[i : i + 16]
                vals = ", ".join(f"0x{v:04X}" for v in chunk)
                f.write(f"        {vals},\n")
            f.write("    },\n")

        f.write("};\n")


def verify_table(table):
    """验证关键码点映射正确。"""
    checks = [
        (0xB0, 0xA1, 0x554A, "啊"),
        (0xC4, 0xE3, 0x4F60, "你"),
        (0xBA, 0xC3, 0x597D, "好"),
        (0xCA, 0xC0, 0x4E16, "世"),
        (0xBD, 0xE7, 0x754C, "界"),
    ]
    all_ok = True
    for hi, lo, expected_cp, char in checks:
        row = hi - 0x81
        col = gbk_col(lo)
        actual = table[row][col]
        status = "OK" if actual == expected_cp else "FAIL"
        if actual != expected_cp:
            all_ok = False
        print(
            f"  {status}: GBK 0x{hi:02X}{lo:02X} → "
            f"U+{actual:04X} (expected U+{expected_cp:04X} '{char}')"
        )
    return all_ok


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    output_path = os.path.join(
        project_root, "components", "text_encoding", "gbk_table.c"
    )

    # 允许通过命令行参数指定输出路径
    if len(sys.argv) > 1:
        output_path = sys.argv[1]

    print("Generating GBK→Unicode mapping table...")
    table = generate_table()

    # 统计有效映射数
    count = sum(1 for row in table for val in row if val != 0)
    print(f"  Valid mappings: {count}")

    print("Verifying key codepoints...")
    if not verify_table(table):
        print("ERROR: Verification failed!")
        sys.exit(1)

    # 确保输出目录存在
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    print(f"Writing {output_path}...")
    write_c_file(table, output_path)

    # 计算文件大小
    size_kb = os.path.getsize(output_path) / 1024
    print(f"Done! Output size: {size_kb:.1f} KB")


if __name__ == "__main__":
    main()
