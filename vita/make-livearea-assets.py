#!/usr/bin/env python3
"""Generate the PS Vita LiveArea assets from the project icon.

The Vita's package installer rejects LiveArea images that are not 8-bit
palette PNGs -- VitaShell reports 0x8010113D at the end of the install --
which is why every image here goes through a quantisation pass. Regenerating
these by hand, or with a tool that writes 24/32-bit PNGs, breaks installation
even though the VPK itself is perfectly well formed.

Usage: python3 vita/make-livearea-assets.py
"""

import pathlib
import sys

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parent.parent
ICON = ROOT / "icon.png"
SCE_SYS = ROOT / "vita" / "sce_sys"
CONTENTS = SCE_SYS / "livearea" / "contents"

# Sizes Sony's LiveArea spec asks for.
ICON0_SIZE = (128, 128)
BG_SIZE = (840, 500)
STARTUP_SIZE = (280, 158)

# The game's grass, top to bottom.
LAWN_TOP = (46, 94, 40)
LAWN_BOTTOM = (19, 46, 22)


def lawn_backdrop(size):
    """A soft vertical gradient in the same family as the game's lawn."""
    width, height = size
    backdrop = Image.new("RGB", size)
    draw = ImageDraw.Draw(backdrop)
    for y in range(height):
        t = y / max(height - 1, 1)
        draw.line(
            [(0, y), (width, y)],
            fill=tuple(
                int(LAWN_TOP[i] + (LAWN_BOTTOM[i] - LAWN_TOP[i]) * t) for i in range(3)
            ),
        )
    return backdrop


def place(icon, size, art_size, offset=None):
    """Composite the icon over a lawn backdrop at the requested size."""
    canvas = lawn_backdrop(size)
    art = icon.resize(art_size, Image.LANCZOS)
    if offset is None:
        offset = ((size[0] - art_size[0]) // 2, (size[1] - art_size[1]) // 2)
    canvas.paste(art, offset, art)
    return canvas


def save_8bit(image, path):
    """Write an 8-bit palette PNG, which is what the installer accepts."""
    path.parent.mkdir(parents=True, exist_ok=True)
    quantised = image.convert(
        "P", palette=Image.ADAPTIVE, colors=256, dither=Image.FLOYDSTEINBERG
    )
    quantised.save(path, optimize=True)

    # Fail loudly rather than shipping a VPK that cannot be installed.
    written = Image.open(path)
    if written.mode != "P":
        sys.exit(f"{path} came out as {written.mode}, expected an 8-bit palette")
    print(f"{path.relative_to(ROOT)}: {written.mode} {written.size}")


def main():
    icon = Image.open(ICON).convert("RGBA")

    # The bubble on the home screen.
    save_8bit(place(icon, ICON0_SIZE, (118, 118)), SCE_SYS / "icon0.png")

    # The LiveArea backdrop, with the art sitting to the right of the frame.
    save_8bit(
        place(icon, BG_SIZE, (380, 380), (BG_SIZE[0] - 380 - 48, (BG_SIZE[1] - 380) // 2)),
        CONTENTS / "bg.png",
    )

    # The artwork inside the "start" gate.
    save_8bit(place(icon, STARTUP_SIZE, (132, 132)), CONTENTS / "startup.png")


if __name__ == "__main__":
    main()
