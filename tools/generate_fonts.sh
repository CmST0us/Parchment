#!/bin/bash
# 字体生成脚本：生成 UI 字体和阅读字体（均为 .pfnt 格式）
#
# 用法: ./tools/generate_fonts.sh <ui-font-file> <reading-font-file>
#
# 输出:
#   UI 字体:   fonts_data/ui_font_{16,24}.pfnt  (ASCII + GB2312 一级)
#   阅读字体:  fonts_data/reading_{24,32}.pfnt   (GB18030)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FONTCONVERT="$SCRIPT_DIR/fontconvert.py"

OUTPUT_DIR="$PROJECT_DIR/fonts_data"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <ui-font-file> <reading-font-file>"
    echo "  ui-font-file:      e.g. LXGWWenKaiMono-Regular.ttf"
    echo "  reading-font-file:  e.g. LXGWWenKai-Regular.ttf"
    exit 1
fi

UI_FONT_FILE="$1"
READING_FONT_FILE="$2"

if [ ! -f "$UI_FONT_FILE" ]; then
    echo "Error: UI font file not found: $UI_FONT_FILE"
    exit 1
fi
if [ ! -f "$READING_FONT_FILE" ]; then
    echo "Error: Reading font file not found: $READING_FONT_FILE"
    exit 1
fi

# 确保输出目录存在（清空旧文件）
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

echo "================================================"
echo "  Parchment Font Generator"
echo "  UI font:      $UI_FONT_FILE"
echo "  Reading font:  $READING_FONT_FILE"
echo "================================================"
echo ""

# ── UI 字体 (.pfnt binary, ASCII + GB2312 一级) ──
UI_SIZES=(16 24)

echo "── Generating UI fonts (ASCII + GB2312 Level 1) ──"
for SIZE in "${UI_SIZES[@]}"; do
    NAME="ui_font_${SIZE}"
    OUTPUT="$OUTPUT_DIR/${NAME}.pfnt"
    echo "  ${NAME} (${SIZE}px) → ${OUTPUT}"
    python3 "$FONTCONVERT" "$NAME" "$SIZE" "$UI_FONT_FILE" \
        --compress --output-format pfnt --charset gb2312-1 > "$OUTPUT"
    # 显示文件大小
    FILE_SIZE=$(stat -f%z "$OUTPUT" 2>/dev/null || stat --printf="%s" "$OUTPUT" 2>/dev/null || echo "?")
    echo "  → ${FILE_SIZE} bytes"
done
echo "  Done: ${#UI_SIZES[@]} UI font files."
echo ""

# ── 阅读字体 (.pfnt binary, GB2312 一级+二级 + 日语) ──
READING_SIZES=(24 32)

echo "── Generating reading fonts (GB2312 + Japanese) ──"
for SIZE in "${READING_SIZES[@]}"; do
    NAME="reading_${SIZE}"
    OUTPUT="$OUTPUT_DIR/${NAME}.pfnt"
    echo "  ${NAME} (${SIZE}px) → ${OUTPUT}"
    python3 "$FONTCONVERT" "$NAME" "$SIZE" "$READING_FONT_FILE" \
        --compress --output-format pfnt --charset gb2312+ja > "$OUTPUT"
    # 显示文件大小
    FILE_SIZE=$(stat -f%z "$OUTPUT" 2>/dev/null || stat --printf="%s" "$OUTPUT" 2>/dev/null || echo "?")
    echo "  → ${FILE_SIZE} bytes"
done
echo "  Done: ${#READING_SIZES[@]} reading font files."
echo ""

echo "================================================"
echo "  Generation complete!"
echo "  All fonts:  $OUTPUT_DIR/"
echo "================================================"
