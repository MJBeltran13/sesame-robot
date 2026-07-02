#!/usr/bin/env python3
"""Generate a consistent 128x128 OLED face set for Sesame."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
FACES_DIR = ROOT / "generated-oled-art" / "faces"
W = H = 128
BLACK = 0
WHITE = 255


def base() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("L", (W, H), BLACK)
    draw = ImageDraw.Draw(img)

    # Shared robot silhouette. Keep this identical for every expression.
    draw.rounded_rectangle((15, 25, 113, 110), radius=13, outline=WHITE, width=5, fill=BLACK)
    draw.rounded_rectangle((16, 26, 112, 109), radius=12, outline=WHITE, width=2)
    draw.rounded_rectangle((5, 56, 15, 83), radius=4, fill=WHITE)
    draw.rounded_rectangle((113, 56, 123, 83), radius=4, fill=WHITE)
    draw.rectangle((61, 12, 67, 25), fill=WHITE)
    draw.rounded_rectangle((49, 19, 79, 27), radius=4, fill=WHITE)
    draw.ellipse((55, 3, 73, 21), fill=WHITE)

    # Shared cheeks.
    draw.rounded_rectangle((32, 77, 43, 83), radius=2, fill=WHITE)
    draw.rounded_rectangle((85, 77, 96, 83), radius=2, fill=WHITE)
    return img, draw


def ring_eye(draw: ImageDraw.ImageDraw, cx: int, cy: int, outer: int = 13, inner: int = 7) -> None:
    draw.ellipse((cx - outer, cy - outer, cx + outer, cy + outer), fill=WHITE)
    draw.ellipse((cx - inner, cy - inner, cx + inner, cy + inner), fill=BLACK)


def dot_eye(draw: ImageDraw.ImageDraw, cx: int, cy: int, r: int = 8) -> None:
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WHITE)


def arc(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], start: int, end: int, width: int = 5) -> None:
    draw.arc(box, start=start, end=end, fill=WHITE, width=width)


def smile(draw: ImageDraw.ImageDraw) -> None:
    arc(draw, (43, 67, 85, 96), 20, 160, 5)


def sad_mouth(draw: ImageDraw.ImageDraw) -> None:
    arc(draw, (43, 81, 85, 105), 200, 340, 5)


def flat_mouth(draw: ImageDraw.ImageDraw) -> None:
    draw.rounded_rectangle((51, 86, 77, 91), radius=2, fill=WHITE)


def open_mouth(draw: ImageDraw.ImageDraw) -> None:
    draw.ellipse((54, 78, 74, 100), fill=WHITE)
    draw.ellipse((59, 83, 69, 95), fill=BLACK)


def big_mouth(draw: ImageDraw.ImageDraw) -> None:
    draw.pieslice((45, 70, 83, 104), 0, 180, fill=WHITE)


def sleepy_bubbles(draw: ImageDraw.ImageDraw) -> None:
    draw.ellipse((83, 35, 91, 43), outline=WHITE, width=3)
    draw.ellipse((94, 24, 106, 36), outline=WHITE, width=3)
    draw.ellipse((109, 12, 123, 26), outline=WHITE, width=3)


def thought_bubbles(draw: ImageDraw.ImageDraw) -> None:
    draw.ellipse((82, 34, 90, 42), outline=WHITE, width=3)
    draw.ellipse((93, 24, 105, 36), outline=WHITE, width=3)
    draw.ellipse((108, 11, 124, 27), outline=WHITE, width=3)


def save(label: str, img: Image.Image) -> None:
    FACES_DIR.mkdir(parents=True, exist_ok=True)
    img.save(FACES_DIR / f"sesame-face-{label}-128.png")
    img.resize((512, 512), Image.Resampling.NEAREST).save(FACES_DIR / f"sesame-face-{label}-source.png")


def draw_default() -> Image.Image:
    img, d = base()
    ring_eye(d, 46, 59)
    ring_eye(d, 82, 59)
    flat_mouth(d)
    return img


def draw_happy() -> Image.Image:
    img, d = base()
    arc(d, (34, 49, 58, 69), 200, 340, 5)
    arc(d, (70, 49, 94, 69), 200, 340, 5)
    big_mouth(d)
    return img


def draw_sad() -> Image.Image:
    img, d = base()
    arc(d, (34, 53, 58, 74), 20, 160, 5)
    arc(d, (70, 53, 94, 74), 20, 160, 5)
    sad_mouth(d)
    return img


def draw_angry() -> Image.Image:
    img, d = base()
    d.polygon([(32, 47), (59, 55), (58, 66), (37, 60)], fill=WHITE)
    d.polygon([(96, 47), (69, 55), (70, 66), (91, 60)], fill=WHITE)
    sad_mouth(d)
    return img


def draw_surprised() -> Image.Image:
    img, d = base()
    ring_eye(d, 46, 59, 14, 7)
    ring_eye(d, 82, 59, 14, 7)
    open_mouth(d)
    return img


def draw_sleepy() -> Image.Image:
    img, d = base()
    d.rounded_rectangle((34, 61, 58, 66), radius=2, fill=WHITE)
    d.rounded_rectangle((70, 61, 94, 66), radius=2, fill=WHITE)
    open_mouth(d)
    sleepy_bubbles(d)
    return img


def draw_love() -> Image.Image:
    img, d = base()
    for cx in (46, 82):
        d.polygon(
            [(cx, 70), (cx - 16, 56), (cx - 12, 45), (cx, 50), (cx + 12, 45), (cx + 16, 56)],
            fill=WHITE,
        )
    smile(d)
    return img


def draw_confused() -> Image.Image:
    img, d = base()
    ring_eye(d, 46, 61, 13, 7)
    ring_eye(d, 82, 61, 13, 7)
    sad_mouth(d)
    arc(d, (30, 38, 56, 55), 200, 360, 5)
    d.ellipse((45, 56, 53, 64), fill=WHITE)
    thought_bubbles(d)
    return img


def draw_thinking() -> Image.Image:
    img, d = base()
    ring_eye(d, 46, 60, 12, 6)
    ring_eye(d, 82, 60, 12, 6)
    flat_mouth(d)
    d.polygon([(43, 98), (50, 90), (64, 87), (77, 88), (82, 94), (68, 96), (55, 100)], fill=WHITE)
    thought_bubbles(d)
    return img


def draw_excited() -> Image.Image:
    img, d = base()
    d.polygon([(31, 62), (46, 43), (61, 62), (46, 72)], fill=WHITE)
    d.polygon([(67, 62), (82, 43), (97, 62), (82, 72)], fill=WHITE)
    big_mouth(d)
    d.rectangle((61, 34, 67, 45), fill=WHITE)
    d.line((49, 38, 43, 30), fill=WHITE, width=4)
    d.line((79, 38, 85, 30), fill=WHITE, width=4)
    return img


def draw_dead() -> Image.Image:
    img, d = base()
    for cx in (46, 82):
        d.line((cx - 11, 49, cx + 11, 71), fill=WHITE, width=6)
        d.line((cx + 11, 49, cx - 11, 71), fill=WHITE, width=6)
    flat_mouth(d)
    return img


def draw_wink() -> Image.Image:
    img, d = base()
    ring_eye(d, 46, 59)
    arc(d, (70, 50, 94, 68), 200, 340, 5)
    smile(d)
    return img


DRAWERS = {
    "default": draw_default,
    "happy": draw_happy,
    "sad": draw_sad,
    "angry": draw_angry,
    "surprised": draw_surprised,
    "sleepy": draw_sleepy,
    "love": draw_love,
    "confused": draw_confused,
    "thinking": draw_thinking,
    "excited": draw_excited,
    "dead": draw_dead,
    "wink": draw_wink,
}


def main() -> None:
    for label, drawer in DRAWERS.items():
        save(label, drawer())
    print(f"Generated {len(DRAWERS)} consistent faces in {FACES_DIR}")


if __name__ == "__main__":
    main()
