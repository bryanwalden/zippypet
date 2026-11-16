import os
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUTPUT_PNG_DIR = "assets/sprites/generated"
HEADER_PATH = "include/CeratosaurusSprites.h"
os.makedirs(OUTPUT_PNG_DIR, exist_ok=True)

CHROMA_KEY = 0xF81F # Magenta key

def to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

STAGE_PALETTES = {
    1: { # Baby Cerato: Soft honey peach, warm amber, cream belly, sapphire glossy eye
        'skin_top': (255, 175, 80),
        'skin_mid': (248, 130, 40),
        'skin_dark': (200, 85, 25),
        'skin_shadow': (140, 50, 18),
        'belly_top': (255, 245, 195),
        'belly_mid': (255, 225, 150),
        'belly_dark': (225, 185, 110),
        'horn': (255, 245, 210),
        'horn_shd': (220, 195, 150),
        'eye_iris': (50, 130, 225),
        'eye_pupil': (10, 15, 25),
        'cheek': (255, 110, 90),
        'mouth': (150, 35, 25),
        'tongue': (240, 90, 110),
        'claw': (255, 255, 240),
        'scute': (255, 100, 50),
        'blast_core': (255, 255, 220),
        'blast_mid': (255, 190, 50),
        'blast_edge': (255, 90, 30),
    },
    2: { # Juvenile Cerato: Energetic hunter orange, golden belly, emerald reptile eye
        'skin_top': (255, 160, 50),
        'skin_mid': (236, 110, 25),
        'skin_dark': (180, 65, 15),
        'skin_shadow': (125, 35, 12),
        'belly_top': (255, 235, 165),
        'belly_mid': (245, 210, 125),
        'belly_dark': (210, 165, 80),
        'horn': (250, 245, 220),
        'horn_shd': (205, 185, 140),
        'eye_iris': (30, 215, 110),
        'eye_pupil': (8, 12, 10),
        'cheek': (245, 95, 60),
        'mouth': (135, 28, 18),
        'tongue': (230, 75, 95),
        'claw': (252, 250, 235),
        'scute': (230, 65, 25),
        'blast_core': (255, 255, 200),
        'blast_mid': (255, 160, 35),
        'blast_edge': (235, 60, 20),
    },
    3: { # Rookie Ceratosaurus: Classic ferocious theropod, ivory horn & fangs, golden slit eye
        'skin_top': (250, 140, 35),
        'skin_mid': (220, 90, 20),
        'skin_dark': (160, 48, 15),
        'skin_shadow': (105, 25, 10),
        'belly_top': (250, 220, 145),
        'belly_mid': (235, 195, 110),
        'belly_dark': (190, 145, 70),
        'horn': (248, 244, 225),
        'horn_shd': (195, 175, 130),
        'eye_iris': (255, 195, 25),
        'eye_pupil': (5, 5, 5),
        'cheek': (220, 70, 40),
        'mouth': (115, 18, 14),
        'tongue': (220, 65, 80),
        'claw': (248, 245, 230),
        'scute': (180, 40, 15),
        'blast_core': (255, 250, 190),
        'blast_mid': (255, 130, 25),
        'blast_edge': (210, 40, 15),
    },
    4: { # Champion Pyro Cerato: Volcanic crimson, burning magma horn, obsidian armor scutes
        'skin_top': (240, 80, 30),
        'skin_mid': (195, 38, 20),
        'skin_dark': (140, 22, 15),
        'skin_shadow': (75, 12, 10),
        'belly_top': (250, 160, 50),
        'belly_mid': (220, 110, 30),
        'belly_dark': (165, 60, 20),
        'horn': (255, 220, 50),
        'horn_shd': (220, 120, 25),
        'eye_iris': (255, 45, 25),
        'eye_pupil': (12, 5, 5),
        'cheek': (180, 30, 25),
        'mouth': (80, 10, 8),
        'tongue': (210, 50, 60),
        'claw': (60, 40, 45),
        'scute': (50, 32, 38),
        'blast_core': (255, 255, 180),
        'blast_mid': (255, 140, 30),
        'blast_edge': (200, 25, 15),
    },
    5: { # Ultimate Flare Cerato: Basalt carapace, glowing orange magma veins, solar blade horn
        'skin_top': (125, 95, 105),
        'skin_mid': (85, 55, 62),
        'skin_dark': (50, 30, 35),
        'skin_shadow': (20, 12, 18),
        'belly_top': (255, 150, 35),
        'belly_mid': (225, 75, 20),
        'belly_dark': (155, 35, 15),
        'horn': (255, 245, 120),
        'horn_shd': (255, 140, 25),
        'eye_iris': (255, 215, 45),
        'eye_pupil': (8, 4, 6),
        'cheek': (240, 90, 30),
        'mouth': (60, 8, 10),
        'tongue': (200, 40, 45),
        'claw': (40, 25, 30),
        'scute': (140, 110, 120),
        'blast_core': (255, 255, 220),
        'blast_mid': (255, 180, 40),
        'blast_edge': (220, 50, 18),
    },
    6: { # Cyborg Metal Cerato: Polished chrome steel, dark gunmetal, neon cyan laser optic visor
        'skin_top': (190, 210, 230),
        'skin_mid': (115, 135, 160),
        'skin_dark': (65, 80, 100),
        'skin_shadow': (30, 38, 50),
        'belly_top': (210, 120, 55),
        'belly_mid': (175, 80, 35),
        'belly_dark': (120, 50, 22),
        'horn': (215, 235, 255),
        'horn_shd': (100, 140, 180),
        'eye_iris': (0, 240, 255),
        'eye_pupil': (0, 180, 220),
        'cheek': (0, 200, 240),
        'mouth': (35, 42, 55),
        'tongue': (180, 50, 60),
        'claw': (220, 235, 250),
        'scute': (80, 100, 125),
        'blast_core': (220, 255, 255),
        'blast_mid': (0, 230, 255),
        'blast_edge': (0, 140, 220),
    },
    7: { # Mecha Cerato Titan: Midnight titan navy, radiant battle gold trim, white/cyan plasma core
        'skin_top': (140, 175, 230),
        'skin_mid': (70, 95, 145),
        'skin_dark': (40, 55, 95),
        'skin_shadow': (18, 24, 45),
        'belly_top': (255, 225, 90),
        'belly_mid': (220, 175, 45),
        'belly_dark': (155, 115, 25),
        'horn': (240, 250, 255),
        'horn_shd': (0, 210, 240),
        'eye_iris': (0, 255, 240),
        'eye_pupil': (255, 80, 180),
        'cheek': (255, 70, 170),
        'mouth': (20, 26, 42),
        'tongue': (255, 60, 160),
        'claw': (255, 230, 100),
        'scute': (160, 195, 245),
        'blast_core': (255, 255, 255),
        'blast_mid': (0, 255, 245),
        'blast_edge': (255, 50, 160),
    }
}

STAGE_DIMS = {
    1: (46, 42),
    2: (52, 46),
    3: (58, 52),
    4: (64, 56),
    5: (68, 60),
    6: (72, 62),
    7: (76, 66),
}

def render_frame(lvl, action_name, subframe=0):
    pal = STAGE_PALETTES[lvl]
    base_w, base_h = STAGE_DIMS[lvl]
    
    extra_w = 28 if action_name.startswith('roar') else (12 if (action_name.startswith('eat') or action_name.startswith('happy')) else 0)
    w = base_w + extra_w
    h = base_h
    
    SCALE = 4
    sw, sh = w * SCALE, h * SCALE
    im = Image.new('RGBA', (sw, sh), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)

    # Kinematics per subframe
    # Walk 0..3: contact L, passing bob, contact R, passing bob
    walk_bobs = [0, int(1.8 * SCALE), 0, -int(1.2 * SCALE)]
    tail_sways = [0, int(2.5 * SCALE), 0, -int(2.5 * SCALE)]
    
    if action_name == 'walk':
        cur_y = walk_bobs[subframe % 4]
        tail_dy = tail_sways[subframe % 4]
        walk_step = subframe % 4
    elif action_name == 'jump':
        cur_y = -int(6.5 * SCALE) if subframe == 0 else -int(2.5 * SCALE)
        tail_dy = int(3.0 * SCALE)
        walk_step = 0
    elif action_name == 'sleep':
        cur_y = 0
        tail_dy = 0
        walk_step = 0
    elif action_name == 'roar':
        cur_y = int(0.8 * SCALE)
        tail_dy = -int(2.0 * SCALE)
        walk_step = 0
    elif action_name == 'eat':
        cur_y = int(1.2 * SCALE) if subframe == 1 else 0
        tail_dy = 0
        walk_step = 0
    elif action_name == 'happy':
        cur_y = -int(4.0 * SCALE) if subframe == 1 else 0
        tail_dy = int(4.0 * SCALE) * (1 if subframe == 1 else -1)
        walk_step = 0
    else:
        cur_y = 0
        tail_dy = 0
        walk_step = 0

    # SLEEP
    if action_name == 'sleep':
        breath_y = int(1.5 * SCALE) if subframe == 1 else 0
        # Curled body
        draw.ellipse([sw*0.20, sh*0.48 - breath_y, sw*0.75, sh*0.95], fill=pal['skin_mid'])
        draw.ellipse([sw*0.24, sh*0.52 - breath_y, sw*0.72, sh*0.92], fill=pal['skin_dark'])
        draw.ellipse([sw*0.35, sh*0.58 - breath_y, sw*0.70, sh*0.90], fill=pal['belly_mid'])
        # Head down
        draw.ellipse([sw*0.48, sh*0.52, sw*0.88, sh*0.88], fill=pal['skin_top'])
        draw.ellipse([sw*0.52, sh*0.55, sw*0.84, sh*0.85], fill=pal['skin_mid'])
        # Horn
        draw.polygon([(sw*0.78, sh*0.60), (sw*0.90, sh*0.50), (sw*0.84, sh*0.70)], fill=pal['horn'])
        # Sleeping eye slit: ^
        eye_cx, eye_cy = sw * 0.65, sh * 0.65
        draw.line([(eye_cx - 8, eye_cy + 4), (eye_cx, eye_cy), (eye_cx + 8, eye_cy + 4)], fill=pal['mouth'], width=3)
        # Tail curled around front
        draw.arc([sw*0.08, sh*0.60, sw*0.38, sh*0.96], start=45, end=270, fill=pal['skin_mid'], width=int(SCALE*2.8))
        draw.arc([sw*0.10, sh*0.62, sw*0.36, sh*0.94], start=45, end=270, fill=pal['skin_top'], width=int(SCALE*1.2))
        # Dorsal scutes
        for sc_x, sc_y in [(0.28, 0.44), (0.38, 0.42), (0.48, 0.44)]:
            draw.polygon([(sw*sc_x, sh*sc_y - breath_y), (sw*(sc_x+0.04), sh*(sc_y-0.08) - breath_y), (sw*(sc_x+0.08), sh*sc_y - breath_y)], fill=pal['scute'])
        # Zzz drifting up
        z_offset = int(4.0 * SCALE) if subframe == 1 else 0
        for zx, zy, sz in [(0.75, 0.35 - z_offset*0.005, 14), (0.82, 0.22 - z_offset*0.008, 20), (0.72, 0.10 - z_offset*0.012, 26)]:
            draw.line([(sw*zx, sh*zy), (sw*zx + sz, sh*zy)], fill=(130, 190, 255, 230), width=4)
            draw.line([(sw*zx + sz, sh*zy), (sw*zx, sh*zy + sz)], fill=(130, 190, 255, 230), width=4)
            draw.line([(sw*zx, sh*zy + sz), (sw*zx + sz, sh*zy + sz)], fill=(130, 190, 255, 230), width=4)

        return im.resize((w, h), Image.Resampling.LANCZOS)

    # 1. TAIL
    tail_poly = [
        (sw*0.06, sh*0.52 + cur_y + tail_dy),
        (sw*0.16, sh*0.48 + cur_y + tail_dy*0.7),
        (sw*0.32, sh*0.54 + cur_y),
        (sw*0.28, sh*0.70 + cur_y),
        (sw*0.12, sh*0.66 + cur_y + tail_dy)
    ]
    draw.polygon(tail_poly, fill=pal['skin_mid'])
    draw.line(tail_poly[:2], fill=pal['skin_top'], width=int(SCALE*1.5))
    # Tail scutes
    for sc_x, sc_y in [(0.12, 0.48), (0.18, 0.49), (0.24, 0.51)]:
        draw.polygon([(sw*sc_x, sh*sc_y + cur_y + tail_dy*0.5), (sw*(sc_x+0.03), sh*(sc_y-0.05) + cur_y + tail_dy*0.5), (sw*(sc_x+0.05), sh*sc_y + cur_y + tail_dy*0.5)], fill=pal['scute'])

    # 2. TORSO & BELLY
    draw.ellipse([sw*0.22, sh*0.36 + cur_y, sw*0.65, sh*0.78 + cur_y], fill=pal['skin_mid'])
    draw.ellipse([sw*0.24, sh*0.34 + cur_y, sw*0.62, sh*0.55 + cur_y], fill=pal['skin_top'])
    draw.ellipse([sw*0.25, sh*0.60 + cur_y, sw*0.60, sh*0.82 + cur_y], fill=pal['skin_dark'])
    
    # Ventral underbelly
    draw.ellipse([sw*0.34, sh*0.42 + cur_y, sw*0.62, sh*0.76 + cur_y], fill=pal['belly_mid'])
    draw.ellipse([sw*0.36, sh*0.44 + cur_y, sw*0.58, sh*0.62 + cur_y], fill=pal['belly_top'])
    draw.ellipse([sw*0.36, sh*0.65 + cur_y, sw*0.58, sh*0.78 + cur_y], fill=pal['belly_dark'])

    # Dorsal osteoderm scutes
    for sc_x, sc_y in [(0.28, 0.35), (0.35, 0.32), (0.42, 0.31), (0.49, 0.32)]:
        sc_poly = [(sw*sc_x, sh*sc_y + cur_y), (sw*(sc_x + 0.035), sh*(sc_y - 0.07) + cur_y), (sw*(sc_x + 0.06), sh*sc_y + cur_y)]
        draw.polygon(sc_poly, fill=pal['scute'])
        draw.line(sc_poly[:2], fill=pal['horn'], width=2)

    # 3. LEGS & FEET (Multi-frame kinematics)
    foot_y = sh * 0.98
    back_leg_x = sw * 0.28
    front_leg_x = sw * 0.44

    if action_name == 'jump':
        # Airborne tuck
        draw.ellipse([back_leg_x, sh*0.60 + cur_y, back_leg_x + sw*0.16, sh*0.80 + cur_y], fill=pal['skin_dark'])
        draw.ellipse([front_leg_x, sh*0.62 + cur_y, front_leg_x + sw*0.16, sh*0.82 + cur_y], fill=pal['skin_mid'])
        for cx in [front_leg_x + sw*0.06, front_leg_x + sw*0.12]:
            draw.polygon([(cx, sh*0.80 + cur_y), (cx + 8, sh*0.85 + cur_y), (cx, sh*0.85 + cur_y)], fill=pal['claw'])
    elif action_name == 'walk':
        # 4-frame walk kinematics
        if walk_step == 0: # Contact L
            draw.ellipse([back_leg_x - sw*0.02, sh*0.56 + cur_y, back_leg_x + sw*0.14, foot_y], fill=pal['skin_shadow'])
            draw.ellipse([front_leg_x + sw*0.02, sh*0.56 + cur_y, front_leg_x + sw*0.18, foot_y], fill=pal['skin_mid'])
            for cl_x in [front_leg_x + 4, front_leg_x + 16, front_leg_x + 28]:
                draw.polygon([(cl_x, foot_y - 10), (cl_x + 8, foot_y), (cl_x, foot_y)], fill=pal['claw'])
        elif walk_step == 1: # Passing L -> R
            draw.ellipse([back_leg_x + sw*0.02, sh*0.56 + cur_y, back_leg_x + sw*0.16, foot_y - 12], fill=pal['skin_shadow'])
            draw.ellipse([front_leg_x, sh*0.56 + cur_y, front_leg_x + sw*0.17, foot_y], fill=pal['skin_mid'])
            for cl_x in [front_leg_x + 2, front_leg_x + 14, front_leg_x + 26]:
                draw.polygon([(cl_x, foot_y - 10), (cl_x + 8, foot_y), (cl_x, foot_y)], fill=pal['claw'])
        elif walk_step == 2: # Contact R
            draw.ellipse([back_leg_x + sw*0.03, sh*0.56 + cur_y, back_leg_x + sw*0.17, foot_y], fill=pal['skin_shadow'])
            draw.ellipse([front_leg_x - sw*0.03, sh*0.56 + cur_y, front_leg_x + sw*0.15, foot_y], fill=pal['skin_mid'])
            for cl_x in [front_leg_x - 6, front_leg_x + 6, front_leg_x + 18]:
                draw.polygon([(cl_x, foot_y - 10), (cl_x + 8, foot_y), (cl_x, foot_y)], fill=pal['claw'])
        else: # Passing R -> L
            draw.ellipse([back_leg_x, sh*0.56 + cur_y, back_leg_x + sw*0.16, foot_y], fill=pal['skin_shadow'])
            draw.ellipse([front_leg_x + sw*0.03, sh*0.56 + cur_y, front_leg_x + sw*0.17, foot_y - 14], fill=pal['skin_mid'])
            for cl_x in [front_leg_x + 6, front_leg_x + 18, front_leg_x + 30]:
                draw.polygon([(cl_x, foot_y - 20), (cl_x + 8, foot_y - 12), (cl_x, foot_y - 12)], fill=pal['claw'])
    else:
        # Neutral stance
        draw.ellipse([back_leg_x, sh*0.56 + cur_y, back_leg_x + sw*0.16, foot_y], fill=pal['skin_shadow'])
        draw.ellipse([front_leg_x, sh*0.56 + cur_y, front_leg_x + sw*0.17, foot_y], fill=pal['skin_mid'])
        for cl_x in [front_leg_x + 2, front_leg_x + 14, front_leg_x + 26]:
            draw.polygon([(cl_x, foot_y - 10), (cl_x + 8, foot_y), (cl_x, foot_y)], fill=pal['claw'])

    # 4. NECK & HEAD
    neck_poly = [
        (sw*0.44, sh*0.45 + cur_y),
        (sw*0.58, sh*0.25 + cur_y),
        (sw*0.70, sh*0.35 + cur_y),
        (sw*0.56, sh*0.55 + cur_y)
    ]
    draw.polygon(neck_poly, fill=pal['skin_mid'])
    draw.line(neck_poly[:2], fill=pal['skin_top'], width=int(SCALE*1.5))

    head_x = sw * 0.46
    head_y = sh * 0.12 + cur_y
    head_w = sw * 0.38
    head_h = sh * 0.34
    
    draw.ellipse([head_x, head_y, head_x + head_w, head_y + head_h], fill=pal['skin_mid'])
    draw.ellipse([head_x + 4, head_y + 2, head_x + head_w - 6, head_y + head_h*0.5], fill=pal['skin_top'])

    # Brow Horn Ridge
    brow_poly = [
        (head_x + head_w*0.28, head_y + head_h*0.20),
        (head_x + head_w*0.42, head_y - head_h*0.12),
        (head_x + head_w*0.54, head_y + head_h*0.22)
    ]
    draw.polygon(brow_poly, fill=pal['scute'])
    draw.line(brow_poly[:2], fill=pal['horn'], width=2)

    # Snout
    draw.ellipse([head_x + head_w*0.45, head_y + head_h*0.28, head_x + head_w*1.08, head_y + head_h*0.82], fill=pal['skin_mid'])
    draw.ellipse([head_x + head_w*0.48, head_y + head_h*0.30, head_x + head_w*0.95, head_y + head_h*0.55], fill=pal['skin_top'])

    # CERATOSAURUS NASAL HORN
    horn_base_x = head_x + head_w * 0.72
    horn_base_y = head_y + head_h * 0.32
    horn_height = head_h * (0.40 if lvl == 1 else (0.55 if lvl <= 3 else 0.75))
    horn_tip_x = horn_base_x + head_w * 0.08
    horn_tip_y = horn_base_y - horn_height
    
    horn_poly = [(horn_base_x - 6, horn_base_y), (horn_tip_x, horn_tip_y), (horn_base_x + 10, horn_base_y + 4)]
    draw.polygon(horn_poly, fill=pal['horn'])
    draw.polygon([(horn_base_x - 6, horn_base_y), (horn_tip_x, horn_tip_y), (horn_base_x, horn_base_y)], fill=pal['horn_shd'])
    draw.line([(horn_tip_x, horn_tip_y), (horn_base_x + 10, horn_base_y + 4)], fill=(255, 255, 255, 220), width=2)

    # EYE
    eye_x = head_x + head_w * 0.28
    eye_y = head_y + head_h * 0.22
    eye_w = head_w * 0.24
    eye_h = head_h * 0.30

    if action_name == 'happy' or (action_name == 'eat' and subframe == 1):
        # Arched happy eyes ^^
        draw.arc([eye_x, eye_y, eye_x + eye_w, eye_y + eye_h], start=180, end=360, fill=pal['mouth'], width=4)
        draw.ellipse([eye_x + 2, eye_y + eye_h*0.4, eye_x + eye_w - 2, eye_y + eye_h*0.8], fill=pal['cheek'])
    else:
        draw.ellipse([eye_x - 2, eye_y - 2, eye_x + eye_w + 2, eye_y + eye_h + 2], fill=pal['skin_shadow'])
        draw.ellipse([eye_x, eye_y, eye_x + eye_w, eye_y + eye_h], fill=pal['eye_iris'])
        if lvl == 1:
            draw.ellipse([eye_x + eye_w*0.25, eye_y + eye_h*0.2, eye_x + eye_w*0.75, eye_y + eye_h*0.8], fill=pal['eye_pupil'])
        else:
            draw.ellipse([eye_x + eye_w*0.38, eye_y + eye_h*0.1, eye_x + eye_w*0.62, eye_y + eye_h*0.9], fill=pal['eye_pupil'])
        draw.ellipse([eye_x + eye_w*0.2, eye_y + eye_h*0.15, eye_x + eye_w*0.5, eye_y + eye_h*0.45], fill=(255, 255, 255, 255))
        draw.ellipse([eye_x + eye_w*0.55, eye_y + eye_h*0.55, eye_x + eye_w*0.75, eye_y + eye_h*0.75], fill=(255, 255, 255, 180))

    # MOUTH / JAWS
    mouth_x = head_x + head_w * 0.50
    mouth_y = head_y + head_h * 0.65

    if action_name == 'eat':
        if subframe == 0:
            # Reaching with open jaws
            jaw_poly = [(mouth_x, mouth_y - 4), (mouth_x + head_w*0.48, mouth_y - 14), (mouth_x + head_w*0.42, mouth_y + 16), (mouth_x, mouth_y + 6)]
            draw.polygon(jaw_poly, fill=pal['mouth'])
            draw.ellipse([mouth_x + 6, mouth_y + 2, mouth_x + head_w*0.35, mouth_y + 14], fill=pal['tongue'])
            for tx in [mouth_x + 10, mouth_x + 22, mouth_x + 34]:
                draw.polygon([(tx, mouth_y - 10), (tx + 5, mouth_y - 3), (tx + 10, mouth_y - 10)], fill=pal['claw'])
            # Food in front
            food_x = mouth_x + head_w * 0.42
            food_y = mouth_y - 6
            draw.ellipse([food_x, food_y, food_x + 28, food_y + 22], fill=(180, 30, 20))
            draw.ellipse([food_x + 6, food_y + 4, food_x + 18, food_y + 12], fill=(245, 120, 100))
            draw.ellipse([food_x + 22, food_y + 10, food_x + 32, food_y + 18], fill=pal['claw'])
        else:
            # Chomping down with chewing particles!
            draw.arc([mouth_x + 4, mouth_y - 4, mouth_x + head_w*0.45, mouth_y + 8], start=20, end=160, fill=pal['mouth'], width=int(SCALE*1.2))
            # Crumbs
            for cb_x, cb_y in [(0.88, 0.60), (0.94, 0.68), (0.86, 0.74)]:
                draw.ellipse([head_x + head_w*cb_x, head_y + head_h*cb_y, head_x + head_w*cb_x + 6, head_y + head_h*cb_y + 6], fill=(220, 50, 30))

    elif action_name == 'roar':
        jaw_poly = [(mouth_x, mouth_y - 8), (mouth_x + head_w*0.48, mouth_y - 20), (mouth_x + head_w*0.46, mouth_y + 24), (mouth_x, mouth_y + 10)]
        draw.polygon(jaw_poly, fill=pal['mouth'])
        for tx in [mouth_x + 10, mouth_x + 24, mouth_x + 38]:
            draw.polygon([(tx, mouth_y - 14), (tx + 6, mouth_y - 3), (tx + 12, mouth_y - 14)], fill=pal['claw'])
            draw.polygon([(tx + 4, mouth_y + 16), (tx + 10, mouth_y + 5), (tx + 16, mouth_y + 16)], fill=pal['claw'])

        blast_x = mouth_x + head_w * 0.46
        blast_y = mouth_y + 2
        blast_len = sw - blast_x - 6

        # Subframe 0: Full laser beam / blast core
        # Subframe 1: Shockwave energy ring expansion
        if lvl == 1:
            draw.ellipse([blast_x + 4, blast_y - 14, blast_x + 34, blast_y + 14], fill=pal['blast_mid'])
            draw.ellipse([blast_x + 8, blast_y - 8, blast_x + 26, blast_y + 8], fill=pal['blast_core'])
            if subframe == 1:
                draw.ellipse([blast_x + 32, blast_y - 20, blast_x + 52, blast_y - 2], fill=pal['blast_edge'])
                draw.ellipse([blast_x + 40, blast_y + 6, blast_x + 58, blast_y + 20], fill=pal['blast_mid'])
        elif lvl <= 5:
            # Dragon fire / magma
            draw.polygon([(blast_x, blast_y - 4), (blast_x + blast_len, blast_y - 28), (blast_x + blast_len + 8, blast_y), (blast_x + blast_len, blast_y + 30), (blast_x, blast_y + 8)], fill=pal['blast_edge'])
            draw.polygon([(blast_x + 6, blast_y - 2), (blast_x + blast_len*0.8, blast_y - 16), (blast_x + blast_len*0.85, blast_y + 2), (blast_x + blast_len*0.75, blast_y + 18), (blast_x + 6, blast_y + 5)], fill=pal['blast_mid'])
            draw.ellipse([blast_x + 4, blast_y - 6, blast_x + blast_len*0.48, blast_y + 8], fill=pal['blast_core'])
            if subframe == 1:
                # Extra shockwave rings
                draw.ellipse([blast_x + blast_len*0.5 - 12, blast_y - 24, blast_x + blast_len*0.5 + 12, blast_y + 24], outline=pal['blast_core'], width=3)
        else:
            # Cyber / Mecha Laser
            draw.rectangle([blast_x, blast_y - 8, blast_x + blast_len, blast_y + 10], fill=pal['blast_edge'])
            draw.rectangle([blast_x + 4, blast_y - 4, blast_x + blast_len, blast_y + 6], fill=pal['blast_mid'])
            draw.rectangle([blast_x + 6, blast_y - 1, blast_x + blast_len, blast_y + 3], fill=pal['blast_core'])
            ring_offset = 12 if subframe == 1 else 0
            for r_x in [blast_x + 20 + ring_offset, blast_x + 60 + ring_offset, blast_x + 95 + ring_offset]:
                draw.ellipse([r_x - 10, blast_y - 20, r_x + 10, blast_y + 22], outline=pal['blast_mid'], width=3)
                draw.ellipse([r_x - 5, blast_y - 14, r_x + 5, blast_y + 16], outline=pal['blast_core'], width=2)
    else:
        draw.arc([mouth_x + 4, mouth_y - 4, mouth_x + head_w*0.45, mouth_y + 8], start=20, end=160, fill=pal['mouth'], width=int(SCALE*0.9))
        if lvl >= 2:
            draw.polygon([(mouth_x + 18, mouth_y + 2), (mouth_x + 22, mouth_y + 10), (mouth_x + 26, mouth_y + 2)], fill=pal['claw'])
            draw.polygon([(mouth_x + 32, mouth_y + 2), (mouth_x + 36, mouth_y + 9), (mouth_x + 40, mouth_y + 2)], fill=pal['claw'])

    # 5. FOREARMS
    arm_x = sw * 0.52
    arm_y = sh * 0.50 + cur_y
    if action_name == 'happy':
        draw.ellipse([arm_x - 2, arm_y - 16, arm_x + sw*0.14, arm_y + 6], fill=pal['skin_top'])
        draw.polygon([(arm_x + sw*0.10, arm_y - 18), (arm_x + sw*0.16, arm_y - 24), (arm_x + sw*0.14, arm_y - 14)], fill=pal['claw'])
        # Sparkling stars
        star_offset = 6 if subframe == 1 else 0
        for st_x, st_y in [(0.78, 0.12), (0.86, 0.28), (0.72, 0.32)]:
            draw.line([(sw*st_x, sh*st_y - 8 - star_offset), (sw*st_x, sh*st_y + 8 + star_offset)], fill=pal['horn'], width=3)
            draw.line([(sw*st_x - 8 - star_offset, sh*st_y), (sw*st_x + 8 + star_offset, sh*st_y)], fill=pal['horn'], width=3)
            draw.ellipse([sw*st_x - 3, sh*st_y - 3, sw*st_x + 3, sh*st_y + 3], fill=(255, 255, 255))
    else:
        draw.ellipse([arm_x, arm_y, arm_x + sw*0.13, arm_y + sh*0.12], fill=pal['skin_mid'])
        for tc_i, tc_y in enumerate([arm_y + sh*0.08, arm_y + sh*0.11]):
            draw.polygon([(arm_x + sw*0.10, tc_y), (arm_x + sw*0.16, tc_y + 4), (arm_x + sw*0.11, tc_y + 7)], fill=pal['claw'])

    # Downsample
    res = im.resize((w, h), Image.Resampling.LANCZOS)
    return res

# Generate all multi-frame sequences:
# Walk (4 frames: walk1..walk4), Eat (2: eat1..eat2), Jump (2: jump1..jump2),
# Sleep (2: sleep1..sleep2), Roar (2: roar1..roar2), Happy (2: happy1..happy2)
print("Generating 14-Frame Fluid Animation Suites for All 7 Evolutionary Levels (98 total frames)...")

ANIM_SPECS = [
    ('walk', 4),
    ('eat', 2),
    ('jump', 2),
    ('sleep', 2),
    ('roar', 2),
    ('happy', 2),
]

all_sprites = {}

for lvl in range(1, 8):
    for anim_name, count in ANIM_SPECS:
        for f_idx in range(count):
            frame_label = f"{anim_name}{f_idx + 1}"
            sprite_key = f"cerato_lv{lvl}_{frame_label}"
            sprite_im = render_frame(lvl, anim_name, f_idx)
            all_sprites[sprite_key] = sprite_im
            sprite_im.save(os.path.join(OUTPUT_PNG_DIR, f"{sprite_key}.png"))

print(f"Generated {len(all_sprites)} high-definition frames in {OUTPUT_PNG_DIR}!")

# Write C++ header
print("Writing C++ header CeratosaurusSprites.h...")
with open(HEADER_PATH, "w", encoding="utf-8") as f:
    f.write("#pragma once\n\n")
    f.write("#include <Arduino.h>\n\n")
    f.write("#define SPRITE_CHROMA_KEY 0xF81F  // 16-bit RGB565 Magenta transparency key\n\n")
    f.write("struct SpriteInfo {\n")
    f.write("    const uint16_t* data;\n")
    f.write("    int width;\n")
    f.write("    int height;\n")
    f.write("};\n\n")
    f.write("struct StageSpriteSet {\n")
    f.write("    SpriteInfo walk[4];\n")
    f.write("    SpriteInfo eat[2];\n")
    f.write("    SpriteInfo jump[2];\n")
    f.write("    SpriteInfo sleep[2];\n")
    f.write("    SpriteInfo roar[2];\n")
    f.write("    SpriteInfo happy[2];\n")
    f.write("};\n\n")

    for name, im in all_sprites.items():
        w, h = im.size
        arr = np.array(im)
        f.write(f"// {name}: {w}x{h} pixels\n")
        f.write(f"static const uint16_t PROGMEM {name}_data[{w * h}] = {{\n")
        
        pixel_strs = []
        for y in range(h):
            for x in range(w):
                r, g, b, a = arr[y, x]
                if a < 110:
                    val = CHROMA_KEY
                else:
                    val = to_rgb565(r, g, b)
                    if val == CHROMA_KEY:
                        val = 0xF81E
                pixel_strs.append(f"0x{val:04X}")

        for i in range(0, len(pixel_strs), 12):
            chunk = pixel_strs[i:i+12]
            f.write("    " + ", ".join(chunk))
            if i + 12 < len(pixel_strs):
                f.write(",\n")
            else:
                f.write("\n")
        f.write("};\n")
        f.write(f"static const SpriteInfo {name} = {{ {name}_data, {w}, {h} }};\n\n")

    for lvl in range(1, 8):
        f.write(f"static const StageSpriteSet stage_cerato_lv{lvl} = {{\n")
        f.write(f"    {{ cerato_lv{lvl}_walk1, cerato_lv{lvl}_walk2, cerato_lv{lvl}_walk3, cerato_lv{lvl}_walk4 }},\n")
        f.write(f"    {{ cerato_lv{lvl}_eat1, cerato_lv{lvl}_eat2 }},\n")
        f.write(f"    {{ cerato_lv{lvl}_jump1, cerato_lv{lvl}_jump2 }},\n")
        f.write(f"    {{ cerato_lv{lvl}_sleep1, cerato_lv{lvl}_sleep2 }},\n")
        f.write(f"    {{ cerato_lv{lvl}_roar1, cerato_lv{lvl}_roar2 }},\n")
        f.write(f"    {{ cerato_lv{lvl}_happy1, cerato_lv{lvl}_happy2 }}\n")
        f.write("};\n\n")

    f.write("inline const StageSpriteSet& getStageSprites(uint8_t stage) {\n")
    f.write("    switch (stage) {\n")
    f.write("        case 1: return stage_cerato_lv1;\n")
    f.write("        case 2: return stage_cerato_lv2;\n")
    f.write("        case 3: return stage_cerato_lv3;\n")
    f.write("        case 4: return stage_cerato_lv4;\n")
    f.write("        case 5: return stage_cerato_lv5;\n")
    f.write("        case 6: return stage_cerato_lv6;\n")
    f.write("        case 7: return stage_cerato_lv7;\n")
    f.write("        default: return stage_cerato_lv3;\n")
    f.write("    }\n")
    f.write("}\n\n")

    # Backward compatibility aliases
    for lvl in range(1, 8):
        f.write(f"#define cerato_lv{lvl} cerato_lv{lvl}_walk1\n")
        f.write(f"#define cerato_lv{lvl}_idle1 cerato_lv{lvl}_walk1\n")
        f.write(f"#define cerato_lv{lvl}_idle2 cerato_lv{lvl}_walk2\n")
        f.write(f"#define cerato_lv{lvl}_eat   cerato_lv{lvl}_eat1\n")
        f.write(f"#define cerato_lv{lvl}_jump  cerato_lv{lvl}_jump1\n")
        f.write(f"#define cerato_lv{lvl}_sleep cerato_lv{lvl}_sleep1\n")
        f.write(f"#define cerato_lv{lvl}_roar  cerato_lv{lvl}_roar1\n")
        f.write(f"#define cerato_lv{lvl}_happy cerato_lv{lvl}_happy1\n")

print("Header generation complete!")
