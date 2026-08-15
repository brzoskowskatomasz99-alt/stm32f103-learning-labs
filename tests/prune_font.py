# -*- coding: utf-8 -*-
"""裁剪 FontDotMatrix16 字库到界面所需字形，适配 Keil 试用版 32KB 镜像限制。"""
import io
import re

SRC = r'E:\TEMPLATE\Template\Core\Inc\FontDotMatrix16.c'
HDR = r'E:\TEMPLATE\Template\Core\Inc\FontDotMatrix16.h'
src = io.open(SRC, encoding='utf-8').read()

needed = set()
needed.update(' ABCDEFGHIJKLMNOPQRSTUVWXYZ')
needed.update('0123456789')
needed.update(':% -./_')
needed.update('粤嵌科技温度湿光照土壤')
print('NEEDED_COUNT=', len(needed))

# index 数组（兼容任意数量）
m = re.search(r'g_font_dot_matrix_16_index\[(\d+)\]\s*=\s*\{(.*?)\};', src,
              re.S)
assert m, 'index array not found'
print('INDEX_COUNT=', m.group(1))
index_entries = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2))
print('INDEX_ENTRIES=', len(index_entries))

# 数据段：按 { 32x0x.. } 块提取（顺序与 index 一致）
data_start = src.index('g_font_dot_matrix_16[')
dsec = src[data_start:]
blocks = re.findall(r'\{\s*((?:0x[0-9A-Fa-f]{2}\s*,?\s*){32})\}\s*,?', dsec)
print('DATA_BLOCKS=', len(blocks))
assert len(blocks) == len(index_entries), 'block/index count mismatch'
glyph_data = {}
for glyph, block in zip(index_entries, blocks):
    glyph_data[glyph] = re.findall(r'0x[0-9A-Fa-f]{2}', block)
print('DATA_GLYPHS=', len(glyph_data))

selected = [(g, glyph_data[g]) for g in index_entries if g in needed]
print('SELECTED=', len(selected))

lines = []
lines.append('/* pruned 16x16 dot-matrix font (ASCII-only source).')
lines.append(' * Index glyphs with non-ASCII chars are written as \\xXX escaped')
lines.append(' * UTF-8 byte sequences so ARMCC5 can compile them safely. */')
lines.append('')
lines.append('extern const char *g_font_dot_matrix_16_index[%d];' % len(selected))
lines.append('extern const char g_font_dot_matrix_16[%d][32];' % len(selected))
lines.append('')
idx_lines = ['const char *g_font_dot_matrix_16_index[%d] = {' % len(selected)]


def c_escape(glyph):
    out = []
    for ch in glyph:
        b = ch.encode('utf-8')
        if len(b) == 1 and 0x20 <= b[0] <= 0x7E and ch not in '"\\':
            out.append(ch)
        else:
            for byte in b:
                out.append('\\x%02X' % byte)
    return ''.join(out)


for glyph, _ in selected:
    idx_lines.append('    "%s",' % c_escape(glyph))
idx_lines.append('};')
data_lines = ['const char g_font_dot_matrix_16[%d][32] = {' % len(selected)]
for i, (glyph, values) in enumerate(selected):
    data_lines.append('    /* index %d */' % i)
    data_lines.append('    { %s }, ' % ', '.join(values))
data_lines.append('};')
out = '\n'.join(lines) + '\n\n' + '\n'.join(idx_lines) + '\n\n' + \
      '\n'.join(data_lines) + '\n'
io.open(SRC, 'w', encoding='ascii', newline='\n').write(out)

hdr = ('/* pruned 16x16 dot-matrix font header (ASCII-only). */\n'
       '#ifndef __FONTDOTMATRIX16_H__\n'
       '#define __FONTDOTMATRIX16_H__\n\n'
       'extern const char* g_font_dot_matrix_16_index[%d];\n\n'
       'extern const char g_font_dot_matrix_16[%d][32];\n\n'
       '#endif\n' % (len(selected), len(selected)))
io.open(HDR, 'w', encoding='ascii', newline='\n').write(hdr)
print('WRITTEN')
