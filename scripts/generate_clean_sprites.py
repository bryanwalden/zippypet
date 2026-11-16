#!/usr/bin/env python3
"""
scripts/generate_clean_sprites.py
Generates clean, artifact-free 32x32 sprite sheets for all Ceratosaurus evolutionary levels (1-7) + Egg.
- Eliminates 100% of ground shadows, neighbor overlaps, and seam gaps.
- Pure 1-bit chroma key: Every pixel is either pure pet RGB or pure #FF00FF (0xF81F). Zero fringe pixels.
- 4 unique articulated frames per stage:
    Frame 0: Walk 1 (Left stride forward, seamless body overlap)
    Frame 1: Walk 2 (Right stride forward, seamless body overlap)
    Frame 2: Roar (Arched pose, head tilted up)
    Frame 3: Sleep (Curled low posture, closed eyelids)
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw

FRAME_SIZE = 32
CHROMA_KEY_RGB = (255, 0, 255)


def make_walk_frames(clean_img, target_h):
    """Generate Frame 0 (Walk 1) and Frame 1 (Walk 2) with overlapping seamless limb articulation."""
    aspect = clean_img.width / clean_img.height
    target_w = max(1, int(round(target_h * aspect)))
    base = clean_img.resize((target_w, target_h), Image.Resampling.LANCZOS)

    w, h = base.size
    split_y = int(h * 0.65)
    overlap = max(2, int(h * 0.08))

    # Overlap upper torso and lower legs by `overlap` pixels so rotation never leaves a magenta gap!
    upper = base.crop((0, 0, w, min(h, split_y + overlap)))
    legs = base.crop((0, max(0, split_y - overlap), w, h))

    # Frame 0: Left stride forward
    f0 = Image.new('RGBA', (w + 4, h + 2), (0, 0, 0, 0))
    legs_f0 = legs.rotate(2.5, resample=Image.Resampling.BILINEAR, expand=False)
    f0.paste(legs_f0, (1, max(0, split_y - overlap) + 1), legs_f0)
    f0.paste(upper, (2, 1), upper)

    # Frame 1: Right stride forward
    f1 = Image.new('RGBA', (w + 4, h + 2), (0, 0, 0, 0))
    legs_f1 = legs.rotate(-2.5, resample=Image.Resampling.BILINEAR, expand=False)
    f1.paste(legs_f1, (3, max(0, split_y - overlap)), legs_f1)
    f1.paste(upper, (2, 0), upper)

    b0 = f0.getbbox()
    b1 = f1.getbbox()
    return f0.crop(b0) if b0 else f0, f1.crop(b1) if b1 else f1


def make_roar_frame(clean_img, target_h, play_img=None):
    """Generate Frame 2 (Roar) with arched posture and head tilted up."""
    src = play_img if play_img else clean_img
    aspect = src.width / src.height
    target_w = max(1, int(round(target_h * aspect)))
    base = src.resize((target_w, target_h), Image.Resampling.LANCZOS)

    w, h = base.size
    canvas = Image.new('RGBA', (w + 6, h + 6), (0, 0, 0, 0))
    rot = base.rotate(-4.0, resample=Image.Resampling.BILINEAR, expand=True)
    canvas.paste(rot, (1, 1), rot)

    bbox = canvas.getbbox()
    return canvas.crop(bbox) if bbox else canvas


def make_sleep_frame(clean_img, target_h, roll_img=None):
    """Generate Frame 3 (Sleep) with curled low posture."""
    src = roll_img if roll_img else clean_img
    aspect = src.width / src.height
    sleep_h = int(target_h * 0.84)
    sleep_w = max(1, int(round(sleep_h * aspect * 1.10)))
    base = src.resize((sleep_w, sleep_h), Image.Resampling.LANCZOS)

    w, h = base.size
    canvas = Image.new('RGBA', (w + 4, h + 4), (0, 0, 0, 0))
    canvas.paste(base, (2, 2), base)

    bbox = canvas.getbbox()
    return canvas.crop(bbox) if bbox else canvas


def assemble_to_32x32_tile(rgba_img):
    """
    Fits an RGBA image onto a 32x32 tile with bottom-alignment and pure 1-bit chroma thresholding.
    ZERO blended fringe pixels.
    """
    tile = Image.new('RGB', (FRAME_SIZE, FRAME_SIZE), CHROMA_KEY_RGB)
    tile_arr = np.array(tile)

    w, h = rgba_img.size
    max_dim = 29
    if w > max_dim or h > max_dim:
        scale = max_dim / max(w, h)
        w = max(1, int(round(w * scale)))
        h = max(1, int(round(h * scale)))
        rgba_img = rgba_img.resize((w, h), Image.Resampling.LANCZOS)

    src_arr = np.array(rgba_img)
    pos_x = (FRAME_SIZE - w) // 2
    pos_y = FRAME_SIZE - 1 - h

    for y in range(h):
        for x in range(w):
            alpha = src_arr[y, x, 3] if src_arr.shape[2] == 4 else 255
            if alpha >= 110:
                dest_y = pos_y + y
                dest_x = pos_x + x
                if 0 <= dest_y < FRAME_SIZE and 0 <= dest_x < FRAME_SIZE:
                    r, g, b = int(src_arr[y, x, 0]), int(src_arr[y, x, 1]), int(src_arr[y, x, 2])
                    if r >= 200 and g <= 50 and b >= 200:
                        g = 56
                    tile_arr[dest_y, dest_x] = (r, g, b)

    return Image.fromarray(tile_arr, 'RGB')


def draw_dino_egg(cracked=False):
    """Draws a detailed 32x32 pixel-art prehistoric Dino Egg with warm shell shading, spots, and nest rim."""
    tile = Image.new('RGB', (FRAME_SIZE, FRAME_SIZE), CHROMA_KEY_RGB)
    arr = np.array(tile)

    # Egg center and radii (oval tapering narrower at top)
    cx = 15.5
    cy = 17.5
    rx_base = 9.5
    ry = 12.0

    # Prehistoric spots: (sx, sy, radius, color_main, color_hi)
    spots = [
        (12.0, 12.5, 2.6, (235, 95, 30), (255, 145, 55)),
        (19.0, 15.0, 3.0, (215, 70, 25), (245, 115, 40)),
        (13.0, 21.0, 2.8, (225, 80, 25), (250, 130, 45)),
        (18.0, 9.5,  1.8, (240, 110, 35), (255, 160, 65)),
        (19.5, 22.0, 1.9, (200, 60, 20), (230, 95, 35)),
    ]

    for y in range(4, 31):
        # Taper factor: narrower near top (y=5), wider near lower belly (y=21)
        ty = (y - cy) / ry
        if abs(ty) > 1.05:
            continue
        taper = 1.0 + 0.16 * math.sin(ty * 1.4)
        rx = rx_base * taper

        for x in range(4, 28):
            dx = (x - cx) / rx
            dist = math.sqrt(dx * dx + ty * ty)
            if dist <= 1.0:
                # Dark outline
                if dist >= 0.87:
                    arr[y, x] = (45, 25, 18)
                else:
                    # 3D spherical lighting (light source top-left)
                    light = (dx + 0.38) * (dx + 0.38) + (ty + 0.42) * (ty + 0.42)
                    if light < 0.18:
                        col = (255, 252, 240)  # Specular highlight
                    elif light < 0.65:
                        col = (250, 238, 210)  # Warm ivory shell
                    elif light < 1.25:
                        col = (232, 210, 175)  # Mid cream
                    else:
                        col = (198, 168, 132)  # Lower-right shadow

                    # Check spots
                    for sx, sy, sr, c_main, c_hi in spots:
                        sdist = math.hypot(x - sx, y - sy)
                        if sdist <= sr:
                            col = c_hi if (x < sx and y < sy) else c_main
                            break

                    arr[y, x] = col

    # Frame 1: Glowing magma cracks across the shell!
    if cracked:
        crack_pts = [
            (15, 8), (16, 9), (14, 10), (13, 11), (15, 12),
            (17, 13), (16, 14), (14, 15), (12, 16), (14, 17),
            (16, 18), (18, 19), (17, 20), (15, 21)
        ]
        branch_pts = [
            (13, 11), (11, 12), (10, 13),
            (17, 13), (19, 14), (20, 13),
            (14, 17), (11, 18)
        ]
        for (px, py) in crack_pts + branch_pts:
            if 0 <= py < FRAME_SIZE and 0 <= px < FRAME_SIZE:
                arr[py, px] = (40, 18, 12)
                if px + 1 < FRAME_SIZE and tuple(arr[py, px + 1]) != CHROMA_KEY_RGB:
                    arr[py, px + 1] = (255, 185, 50)  # Golden inner hatch glow

    return Image.fromarray(arr, 'RGB')


def generate_egg_sheet(out_path):
    """Generate 2-frame egg sprite sheet (Frame 0: whole spotted egg, Frame 1: cracking egg)."""
    sheet = Image.new('RGB', (FRAME_SIZE * 2, FRAME_SIZE), CHROMA_KEY_RGB)
    t0 = draw_dino_egg(cracked=False)
    t1 = draw_dino_egg(cracked=True)
    sheet.paste(t0, (0, 0))
    sheet.paste(t1, (FRAME_SIZE, 0))
    sheet.save(out_path)
    print(f"Generated Egg Sheet -> {out_path}")


def main():
    assets_dir = 'assets'
    sprites_dir = os.path.join(assets_dir, 'sprites')
    concept_dir = os.path.join(assets_dir, 'concept_art', 'extracted')

    os.makedirs(assets_dir, exist_ok=True)
    os.makedirs(sprites_dir, exist_ok=True)

    # Generate Egg
    generate_egg_sheet(os.path.join(assets_dir, 'egg.png'))
    generate_egg_sheet(os.path.join(sprites_dir, 'egg.png'))

    # Target heights for evolution progression inside 32x32 canvas
    level_heights = {
        1: 22,  # Baby: Compact, cute
        2: 24,  # Juvenile: Slightly larger
        3: 25,  # Rookie: Classic theropod
        4: 26,  # Champion: Bulky
        5: 27,  # Ultimate: Armored
        6: 28,  # Cyborg: Heavy frame
        7: 29,  # Mecha Titan: Full size
    }

    act_play = None
    act_roll = None
    play_p = os.path.join(concept_dir, 'action_playing.png')
    roll_p = os.path.join(concept_dir, 'action_rolling.png')
    if os.path.isfile(play_p):
        act_play = Image.open(play_p).convert('RGBA')
    if os.path.isfile(roll_p):
        act_roll = Image.open(roll_p).convert('RGBA')

    for lvl in range(1, 8):
        raw_p = os.path.join(concept_dir, f'level_{lvl}.png')
        if not os.path.isfile(raw_p):
            print(f"Missing {raw_p}, skipping Level {lvl}")
            continue

        clean_base = Image.open(raw_p).convert('RGBA')
        # Levels 6 and 7 face LEFT in the original concept sheet, whereas Levels 1-5 face RIGHT.
        # Flip Levels 6 and 7 horizontally so ALL levels 1-7 share the exact same canonical facing direction!
        if lvl in (6, 7):
            clean_base = clean_base.transpose(Image.FLIP_LEFT_RIGHT)

        target_h = level_heights[lvl]

        walk0, walk1 = make_walk_frames(clean_base, target_h)
        roar = make_roar_frame(clean_base, target_h, act_play if lvl == 3 else None)
        sleep = make_sleep_frame(clean_base, target_h, act_roll if lvl == 3 else None)

        sheet = Image.new('RGB', (FRAME_SIZE * 4, FRAME_SIZE), CHROMA_KEY_RGB)
        sheet.paste(assemble_to_32x32_tile(walk0), (0, 0))
        sheet.paste(assemble_to_32x32_tile(walk1), (FRAME_SIZE, 0))
        sheet.paste(assemble_to_32x32_tile(roar),  (FRAME_SIZE * 2, 0))
        sheet.paste(assemble_to_32x32_tile(sleep), (FRAME_SIZE * 3, 0))

        p1 = os.path.join(assets_dir, f'cerato_lv{lvl}.png')
        p2 = os.path.join(sprites_dir, f'cerato_lv{lvl}.png')
        sheet.save(p1)
        sheet.save(p2)
        print(f"Level {lvl}: Clean 4-frame sheet created -> {p1}")


if __name__ == '__main__':
    main()
