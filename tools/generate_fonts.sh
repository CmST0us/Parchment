#!/bin/bash
# 字体生成脚本：使用 fontconvert.py 为 Parchment 项目生成预编译字体
#
# 用法: ./tools/generate_fonts.sh <path-to-NotoSansCJKsc-Regular.otf>
#
# 输出: components/ui_core/fonts/noto_sans_cjk_{20,24,28,32,36,40}.h

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_DIR/components/ui_core/fonts"
FONTCONVERT="$SCRIPT_DIR/fontconvert.py"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <path-to-NotoSansCJKsc-Regular.otf>"
    exit 1
fi

FONT_FILE="$1"

if [ ! -f "$FONT_FILE" ]; then
    echo "Error: Font file not found: $FONT_FILE"
    exit 1
fi

# 确保输出目录存在
mkdir -p "$OUTPUT_DIR"

# 歌词文本中的全部唯一字符（312 个）+ UI 常用补充字符
# 包含: ASCII, CJK 标点, 平假名, 片假名, CJK 汉字, 全角标点
CHARSET=" ehinsu…　あいうえかがきぎくけげこさしすせそただっつてでとなにねのはばひふへまみめもやよらりるれろわをんイウカキサシスセチヒミャユラリン一万上下不个中为乃么之也了事于些亮人什今仍从以们仰休伙会伝传伴但住何作你便俩倘停僕光内几出到刻前力动努勇動化千即却去发受变口只向否启告哪唱啊因在地堂声处変夜够夢大天太奇奔好如存宏定对寻就届巨已并开弃張当往得心怕息恼悩惘想感成我所手才抓投探揺搏摇放教方无既日早时明昔是時曾有望未本机来某样梦歇歌止正此步气気永没法湛点炽烦热照熱片物现生用白的相看真着知确空索线经绝羞而聚肯胆能脚芒若葉蓝要見见觉言让诉话语说走起路身转轻辟达过还这远述迷迹通遇道遠遥那都里間阐阳陽随雨青音頃頑黎黑鼓齐齿！"

SIZES=(20 24 28 32 36 40)

for SIZE in "${SIZES[@]}"; do
    NAME="noto_sans_cjk_${SIZE}"
    OUTPUT="$OUTPUT_DIR/${NAME}.h"
    echo "Generating ${NAME} (${SIZE}px)..."
    python3 "$FONTCONVERT" "$NAME" "$SIZE" "$FONT_FILE" \
        --compress --string "$CHARSET" > "$OUTPUT"
    echo "  -> $OUTPUT"
done

echo ""
echo "Done! Generated ${#SIZES[@]} font files in $OUTPUT_DIR/"
