#!/usr/bin/env sh
# Regenerates the bold Montserrat fonts in src/ui/ with LVGL's lv_font_conv.
# Needs Node.js. Montserrat is SIL Open Font License:
#   https://github.com/JulietaUla/Montserrat
# Usage: tools/gen_fonts.sh path/to/Montserrat-Bold.ttf
set -e
TTF="${1:?pass the path to Montserrat-Bold.ttf}"
CONV="npx -y lv_font_conv@1.5.3"
COMMON="--bpp 4 --format lvgl --no-compress --lv-include lvgl.h"
$CONV --font "$TTF" -r 0x2D,0x30-0x39 --size 110 $COMMON --lv-font-name font_life_110 -o src/ui/font_life_110.c
$CONV --font "$TTF" -r 0x20-0x7E      --size 24  $COMMON --lv-font-name font_bold_24  -o src/ui/font_bold_24.c
$CONV --font "$TTF" -r 0x20-0x7E      --size 32  $COMMON --lv-font-name font_bold_32  -o src/ui/font_bold_32.c

# Guild emblems from the Mana icon font (SIL OFL 1.1, https://mana.andrewgioia.com;
# the symbols themselves are Wizards of the Coast's). Pass mana.ttf as $2.
# U+E600-E604 = W U B R G mana, U+E90C-E915 = the ten guilds.
if [ -n "$2" ]; then
  $CONV --font "$2" -r 0xE600-0xE604,0xE90C-0xE915 --size 240 $COMMON --lv-font-name font_guild_240 -o src/ui/font_guild_240.c
fi
