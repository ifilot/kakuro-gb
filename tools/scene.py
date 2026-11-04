"""The opening screen: the selected 160x144, four-colour maple artwork.

Palette indices are the DMG shades (light to dark). The Color uses the
same four colours for the whole screen, including the PRESS START prompt.
"""
from pathlib import Path

from PIL import Image

W, H = 160, 144
SOURCE = Path(__file__).resolve().parent.parent / 'assets/intro.png'
COLOURS = (0xF7E7CE, 0xD6BD84, 0xCE5231, 0x18213A)
PRESS_X, PRESS_Y, PRESS_W = 4, 14, 12


def draw() -> list[list[int]]:
    with Image.open(SOURCE) as source:
        if source.size != (W, H):
            raise ValueError('The intro artwork must be exactly 160x144')
        colours = [tuple(c.to_bytes(3, 'big')) for c in COLOURS]
        rgb = source.convert('RGB')
        if set(rgb.getdata()) != set(colours):
            raise ValueError('The intro must use exactly the four scene colours')
        pixels = [colours.index(c) for c in rgb.getdata()]
    image = [pixels[y*W:(y+1)*W] for y in range(H)]
    # The blinking prompt restores paper without erasing the illustration.
    if any(image[y][x] for y in range(PRESS_Y*8, PRESS_Y*8+8)
           for x in range(PRESS_X*8, (PRESS_X+PRESS_W)*8)):
        raise ValueError('The PRESS START area must be blank paper')
    return image
