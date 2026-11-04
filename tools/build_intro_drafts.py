#!/usr/bin/env python3
"""Export intro artwork drafts at 160x144 with exactly four palette colors.

The original imagegen concepts stay in sources/. Native-resolution exports
use area sampling followed by a fixed four-color palette, without dithering.
Requires Pillow. No game sources or ROM assets are changed.
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets/intro-drafts'
COLORS = [(247, 231, 206), (214, 189, 132), (206, 82, 49), (24, 33, 58)]
GREEN = [(224, 248, 208), (136, 192, 112), (52, 104, 86), (8, 24, 32)]
NAMES = ['01-maple-canopy', '02-offset-sun', '03-autumn-frame',
         '04-rising-sun', '05-quiet-maple']


def tile_data(image):
    """Return standard Game Boy 2bpp tiles and a 20x18 map, if they fit."""
    pixels = image.load()
    tiles, tilemap = [], []
    for ty in range(18):
        for tx in range(20):
            data = []
            for y in range(8):
                lo = hi = 0
                for x in range(8):
                    value = pixels[tx*8+x, ty*8+y]
                    lo |= (value & 1) << (7-x)
                    hi |= ((value >> 1) & 1) << (7-x)
                data.extend([lo, hi])
            tile = bytes(data)
            if tile not in tiles:
                tiles.append(tile)
            tilemap.append(tiles.index(tile))
    return tiles, tilemap


def comparison(palette_name, suffix):
    sheet = Image.new('RGB', (1512, 984), '#eee3d0')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('DejaVuSans.ttf', 19)
    for i, name in enumerate(NAMES):
        x, y = 12 + i % 3 * 504, 12 + i // 3 * 492
        draw.text((x, y), name.replace('-', ' ').title(), font=font, fill='#18213a')
        with Image.open(OUT / f'{name}{suffix}.png') as image:
            sheet.paste(image.convert('RGB').resize((480, 432), Image.Resampling.NEAREST), (x, y+32))
    sheet.save(OUT / f'comparison-{palette_name}.png')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    palette = Image.new('P', (1, 1))
    palette.putpalette([v for rgb in COLORS for v in rgb] * 64)
    report = {}
    for name in NAMES:
        with Image.open(OUT / 'sources' / f'{name}.png') as source:
            native = source.convert('RGB').resize((160, 144), Image.Resampling.BOX)
        indexed = native.quantize(palette=palette, dither=Image.Dither.NONE)
        # Normalize any duplicate palette indices to 0..3 for 2bpp export.
        indexed = indexed.point([i % 4 for i in range(256)])
        indexed.putpalette([v for rgb in COLORS for v in rgb])
        indexed.save(OUT / f'{name}.png')
        green = indexed.copy()
        green.putpalette([v for rgb in GREEN for v in rgb])
        green.save(OUT / f'{name}-dmg.png')
        for image, suffix in [(indexed, ''), (green, '-dmg')]:
            actual = set(image.convert('RGB').getdata())
            assert image.size == (160, 144)
            assert len(actual) == 4, (name, actual)
            assert actual == set(GREEN if suffix else COLORS)
            image.resize((640, 576), Image.Resampling.NEAREST).save(OUT / f'{name}{suffix}-4x.png')
        tiles, tilemap = tile_data(indexed)
        if len(tiles) <= 256:
            (OUT / f'{name}.2bpp').write_bytes(b''.join(tiles))
            (OUT / f'{name}.tilemap').write_bytes(bytes(tilemap))
            # Decode every exported tile and check it against the image.
            for y in range(144):
                for x in range(160):
                    tile = tiles[tilemap[(y//8)*20+x//8]]
                    decoded = ((tile[(y%8)*2] >> (7-x%8)) & 1)
                    decoded |= ((tile[(y%8)*2+1] >> (7-x%8)) & 1) << 1
                    assert decoded == indexed.getpixel((x, y))
        report[name] = {'width': 160, 'height': 144, 'used_colors': 4,
                        'palette_rgb': COLORS, 'dmg_palette_rgb': GREEN,
                        'unique_8x8_tiles': len(tiles),
                        'fits_256_tile_bank': len(tiles) <= 256,
                        'tile_export_roundtrip_verified': len(tiles) <= 256}
        print(f'{name}: 160x144, exactly 4 colors, {len(tiles)} unique tiles')
    (OUT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    comparison('color', '')
    comparison('dmg', '-dmg')


if __name__ == '__main__':
    main()
