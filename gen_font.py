import re
from pathlib import Path
import struct

src = Path('gohufont.h').read_text()
vals = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', src)]
data = bytearray(vals)

# PSF2 header parse
magic    = struct.unpack_from('<I', data, 0)[0]
version  = struct.unpack_from('<I', data, 4)[0]
hdrsize  = struct.unpack_from('<I', data, 8)[0]
flags    = struct.unpack_from('<I', data, 12)[0]
length   = struct.unpack_from('<I', data, 16)[0]
charsize = struct.unpack_from('<I', data, 20)[0]
height   = struct.unpack_from('<I', data, 24)[0]
width    = struct.unpack_from('<I', data, 28)[0]

total = hdrsize + length * charsize
font_data = data[:total]

Path('font.psf').write_bytes(font_data)

# C array olarak yaz
with open('font_data.c', 'w') as f:
    f.write('unsigned char font_psf[] = {\n')
    for i, b in enumerate(font_data):
        if i % 16 == 0: f.write('    ')
        f.write(f'0x{b:02x},')
        if i % 16 == 15: f.write('\n')
    f.write('\n};\n')
    f.write(f'unsigned int font_psf_len = {len(font_data)};\n')

print(f"Font: {len(font_data)} bytes, {length} glyphs, {charsize} bytes/glyph, {width}x{height}")
