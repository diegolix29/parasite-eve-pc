"""Turn a picture into a chase-camera wall texture: a 256-colour BMP whose palette entry 0 is
transparent, at most 1024x256 (the mod halves it if the room has less free video memory).

    python make_texture.py INPUT OUTPUT.bmp [--crop x0,y0,x1,y1] [--max-width 1024]
                           [--max-height 256] [--clear-white]

--clear-white makes white or near-white areas touching the picture's edge transparent (sky above a
roof, a white sheet background). Needs Pillow and numpy.
"""
import argparse
from collections import deque

import numpy as np
from PIL import Image


def clear_border_white(rgb, threshold):
    """Mask of near-white pixels connected to the border."""
    h, w, _ = rgb.shape
    white = rgb.min(axis=2) >= threshold
    mask = np.zeros((h, w), bool)
    queue = deque()
    for x in range(w):
        for y in (0, h - 1):
            if white[y, x] and not mask[y, x]:
                mask[y, x] = True
                queue.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if white[y, x] and not mask[y, x]:
                mask[y, x] = True
                queue.append((y, x))
    while queue:
        y, x = queue.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and white[ny, nx] and not mask[ny, nx]:
                mask[ny, nx] = True
                queue.append((ny, nx))
    return mask


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('input')
    ap.add_argument('output')
    ap.add_argument('--crop')
    ap.add_argument('--max-width', type=int, default=1024)
    ap.add_argument('--max-height', type=int, default=256)
    ap.add_argument('--clear-white', action='store_true')
    ap.add_argument('--white-threshold', type=int, default=232)
    a = ap.parse_args()

    im = Image.open(a.input).convert('RGB')
    if a.crop:
        im = im.crop(tuple(int(v) for v in a.crop.split(',')))
    scale = min(1.0, a.max_width / im.width, a.max_height / im.height)
    size = (max(1, round(im.width * scale)), max(1, round(im.height * scale)))
    im = im.resize(size, Image.LANCZOS)
    rgb = np.asarray(im).astype(np.int32)
    clear = clear_border_white(rgb, a.white_threshold) if a.clear_white else np.zeros(rgb.shape[:2], bool)

    # 255 colours for the picture, entry 0 for transparency.
    q = im.quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    idx = np.asarray(q).astype(np.int32) + 1
    idx[clear] = 0
    pal = q.getpalette()[:255 * 3]
    pal = [0, 0, 0] + pal + [0, 0, 0] * (256 - 1 - len(pal) // 3)
    out = Image.fromarray(idx.astype(np.uint8), 'P')
    out.putpalette(pal[:768])
    out.save(a.output, format='BMP')
    print(f'{a.output}: {out.width}x{out.height}, {int(clear.sum())} transparent pixels')


if __name__ == '__main__':
    main()
