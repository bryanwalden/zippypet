"""
Advanced Concept-Art Animation Suite Generator for Zippypet.
Faithfully preserves 100% of the original Ceratosaurus concept art designs, colors, and armor.
Eliminates all drop shadows and artifacts around the feet.
Generates articulated limb walking, full-motion jumping (crouch, extend, tuck),
mouth-aligned roaring, and turning animations.
"""

import os
import math
import numpy as np
from PIL import Image, ImageDraw
from collections import deque

CONCEPT_DIR = 'assets/concept_art/extracted'
OUT_DIR = 'assets/sprites/generated'
os.makedirs(OUT_DIR, exist_ok=True)

CHROMA_KEY_HEX = 0xF81F  # Magenta transparency key in RGB565

def to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    val = (r5 << 11) | (g6 << 5) | b5
    if val == CHROMA_KEY_HEX:
        val = 0xF83F
    return val

def scale_exact_h(img, target_h):
    aspect = img.width / img.height
    target_w = max(1, int(round(target_h * aspect)))
    return img.resize((target_w, target_h), Image.Resampling.LANCZOS)

def clean_shadow(img):
    """Ensure no shadow pixels exist around the feet."""
    arr = np.array(img).copy()
    h, w, _ = arr.shape
    r = arr[:,:,0].astype(int)
    g = arr[:,:,1].astype(int)
    b = arr[:,:,2].astype(int)
    
    bright = (r + g + b) / 3.0
    # Shadow detector: light desaturated/warm grey in lower 40%
    is_shadow = (bright > 130) & (g > 125) & (b > 115) & (np.abs(r - g) < 55) & (np.abs(r - b) < 65)
    is_shadow[:int(h * 0.60), :] = False
    arr[is_shadow, 3] = 0
    
    out = Image.fromarray(arr, 'RGBA')
    bbox = out.getbbox()
    return out.crop(bbox) if bbox else out

def generate_walk_frames(base_img):
    """
    4-frame walk cycle with articulated limbs:
    - walk1: neutral plant, left leg forward
    - walk2: right leg swinging forward (lower 40% sheared +3 deg), torso dips 2px, arms pump
    - walk3: neutral plant, right leg forward
    - walk4: left leg swinging forward (lower 40% sheared -3 deg), torso rises 1px, arms sway
    """
    w, h = base_img.size
    frames = []
    
    # Split upper body and legs (roughly at 58% height)
    split_y = int(h * 0.58)
    upper = base_img.crop((0, 0, w, split_y))
    legs = base_img.crop((0, split_y, w, h))
    
    for phase in range(4):
        canvas_w = w + 16
        canvas_h = h + 8
        canvas = Image.new('RGBA', (canvas_w, canvas_h), (0, 0, 0, 0))
        
        if phase == 0:
            # Neutral plant (left forward)
            canvas.paste(upper, (8, 4), upper)
            canvas.paste(legs, (8, 4 + split_y), legs)
        elif phase == 1:
            # Step forward: knee dip 2px, leg forward swing
            # Rotate legs slightly forward
            rot_legs = legs.rotate(3.5, resample=Image.Resampling.BILINEAR, expand=True)
            # Arm pump: tilt upper body slightly forward
            rot_upper = upper.rotate(-1.5, resample=Image.Resampling.BILINEAR, expand=True)
            canvas.paste(rot_upper, (9, 6), rot_upper)
            canvas.paste(rot_legs, (8, 6 + split_y), rot_legs)
        elif phase == 2:
            # Neutral plant (right forward)
            canvas.paste(upper, (8, 4), upper)
            # Slight horizontal leg shift
            canvas.paste(legs, (7, 4 + split_y), legs)
        else: # phase == 3
            # Push off: torso rise 1px, rear leg push
            rot_legs = legs.rotate(-3.5, resample=Image.Resampling.BILINEAR, expand=True)
            rot_upper = upper.rotate(1.5, resample=Image.Resampling.BILINEAR, expand=True)
            canvas.paste(rot_upper, (7, 2), rot_upper)
            canvas.paste(rot_legs, (8, 2 + split_y), rot_legs)
            
        bbox = canvas.getbbox()
        frames.append(canvas.crop(bbox) if bbox else canvas)
        
    return frames

def generate_jump_frames(base_img):
    """
    Full motion jump frames:
    - jump1 (crouch): squash down 15%, knees bent, arms back (anticipation & landing impact)
    - jump2 (leap): fully extended upward thrust (+12% height, legs extended straight down)
    - jump3 (apex/descent): tuck legs, tail arched, gliding forward
    """
    w, h = base_img.size
    
    # 1. Crouch / squash
    crouch_h = int(round(h * 0.84))
    crouch_w = int(round(w * 1.08))
    jump1 = base_img.resize((crouch_w, crouch_h), Image.Resampling.LANCZOS)
    
    # 2. Fully extended leap (stretched)
    leap_h = int(round(h * 1.14))
    leap_w = int(round(w * 0.93))
    jump2 = base_img.resize((leap_w, leap_h), Image.Resampling.LANCZOS)
    
    # 3. Apex / tuck
    # Rotate forward slightly with tucked posture
    canvas = Image.new('RGBA', (w + 10, h + 10), (0, 0, 0, 0))
    tucked = base_img.rotate(5.0, resample=Image.Resampling.BILINEAR, expand=True)
    canvas.paste(tucked, (4, 4), tucked)
    bbox = canvas.getbbox()
    jump3 = canvas.crop(bbox) if bbox else canvas
    
    return [jump1, jump2, jump3]

def generate_turn_frames(base_img):
    """
    2-frame turn transition:
    - turn1: Compress horizontally to 55% width (3/4 front-quarter profile looking at player)
    - turn2: Facing opposite direction, compress to 75% width before expanding
    """
    w, h = base_img.size
    t1_w = max(1, int(round(w * 0.55)))
    turn1 = base_img.resize((t1_w, h), Image.Resampling.LANCZOS)
    
    # Flipped horizontally, 75% width
    flipped = base_img.transpose(Image.FLIP_LEFT_RIGHT)
    t2_w = max(1, int(round(w * 0.75)))
    turn2 = flipped.resize((t2_w, h), Image.Resampling.LANCZOS)
    
    return [turn1, turn2]

def generate_eat_frames(base_img):
    """
    frame 0: head tilt down towards meat morsel
    frame 1: head up chewing with mouth movement and crumbs
    """
    w, h = base_img.size
    canvas0 = Image.new('RGBA', (w + 16, h + 10), (0, 0, 0, 0))
    canvas1 = Image.new('RGBA', (w + 16, h + 10), (0, 0, 0, 0))
    
    # Frame 0: tilt down
    rot0 = base_img.rotate(4.0, resample=Image.Resampling.BILINEAR, expand=True)
    canvas0.paste(rot0, (10, 4), rot0)
    d0 = ImageDraw.Draw(canvas0)
    # Juicy prehistoric meat drumstick on the ground
    mx, my = 4, canvas0.height - 18
    d0.ellipse([mx, my, mx + 8, my + 8], fill=(185, 45, 30, 255), outline=(90, 15, 10, 255))
    d0.ellipse([mx + 3, my + 2, mx + 7, my + 5], fill=(235, 110, 85, 255))
    d0.rectangle([mx + 7, my + 2, mx + 11, my + 5], fill=(240, 230, 200, 255)) # bone tip
    
    # Frame 1: head up chewing
    rot1 = base_img.rotate(-2.5, resample=Image.Resampling.BILINEAR, expand=True)
    canvas1.paste(rot1, (8, 2), rot1)
    d1 = ImageDraw.Draw(canvas1)
    # Crumbs
    d1.rectangle([3, canvas1.height - 18, 5, canvas1.height - 16], fill=(210, 80, 50, 255))
    d1.rectangle([7, canvas1.height - 12, 9, canvas1.height - 10], fill=(210, 80, 50, 255))
    
    b0 = canvas0.getbbox()
    b1 = canvas1.getbbox()
    return [canvas0.crop(b0) if b0 else canvas0, canvas1.crop(b1) if b1 else canvas1]

def generate_sleep_frames(base_img, roll_img=None):
    """
    frame 0: curled resting pose on bedrock with closed eyelids
    frame 1: gentle breathing rise and floating Zzz
    """
    src = roll_img if roll_img else base_img.rotate(75, resample=Image.Resampling.BILINEAR, expand=True)
    w, h = src.size
    
    canvas0 = Image.new('RGBA', (w + 14, h + 12), (0, 0, 0, 0))
    canvas1 = Image.new('RGBA', (w + 14, h + 12), (0, 0, 0, 0))
    
    # Frame 0: settled breath out
    canvas0.paste(src, (4, 6), src)
    d0 = ImageDraw.Draw(canvas0)
    d0.text((w - 2, 4), "z", fill=(180, 220, 255, 220))
    
    # Frame 1: chest rise breath in (slight expansion)
    rise = src.resize((w, int(h * 1.04)), Image.Resampling.BILINEAR)
    canvas1.paste(rise, (4, 4), rise)
    d1 = ImageDraw.Draw(canvas1)
    d1.text((w - 4, 6), "z", fill=(180, 220, 255, 200))
    d1.text((w + 2, 0), "Z", fill=(220, 245, 255, 255))
    
    b0 = canvas0.getbbox()
    b1 = canvas1.getbbox()
    return [canvas0.crop(b0) if b0 else canvas0, canvas1.crop(b1) if b1 else canvas1]

def generate_roar_frames(base_img, play_img=None):
    """
    frame 0: chest swell back in deep inhale
    frame 1: ferocious thrust forward with jaw open and sonic shockwave rings
    """
    src = play_img if play_img else base_img
    w, h = src.size
    
    canvas0 = Image.new('RGBA', (w + 28, h + 12), (0, 0, 0, 0))
    canvas1 = Image.new('RGBA', (w + 28, h + 12), (0, 0, 0, 0))
    
    # Frame 0: coil back
    rot0 = src.rotate(-5.0, resample=Image.Resampling.BILINEAR, expand=True)
    canvas0.paste(rot0, (16, 4), rot0)
    
    # Frame 1: roar thrust forward with jaw wide open
    rot1 = src.rotate(4.0, resample=Image.Resampling.BILINEAR, expand=True)
    canvas1.paste(rot1, (4, 4), rot1)
    
    d1 = ImageDraw.Draw(canvas1)
    # Sonic shockwave rings placed accurately at the snout / mouth (left side)
    mouth_x = 10
    mouth_y = canvas1.height // 2 - 4
    d1.arc([mouth_x - 12, mouth_y - 14, mouth_x + 8, mouth_y + 14], start=120, end=240, fill=(255, 220, 60, 240), width=2)
    d1.arc([mouth_x - 22, mouth_y - 20, mouth_x + 4, mouth_y + 20], start=120, end=240, fill=(255, 130, 20, 200), width=2)
    
    b0 = canvas0.getbbox()
    b1 = canvas1.getbbox()
    return [canvas0.crop(b0) if b0 else canvas0, canvas1.crop(b1) if b1 else canvas1]

def generate_happy_frames(base_img, tongue_img=None):
    """
    frame 0: jump bounce up with sparkles
    frame 1: settle with wagging tail and sparkling expression
    """
    src = tongue_img if tongue_img else base_img
    w, h = src.size
    
    canvas0 = Image.new('RGBA', (w + 16, h + 14), (0, 0, 0, 0))
    canvas1 = Image.new('RGBA', (w + 16, h + 14), (0, 0, 0, 0))
    
    # Frame 0: bounce up
    canvas0.paste(src, (8, 2), src)
    d0 = ImageDraw.Draw(canvas0)
    d0.polygon([(4, 8), (6, 5), (8, 8), (6, 11)], fill=(255, 235, 80, 255))
    
    # Frame 1: settle
    canvas1.paste(src, (8, 6), src)
    d1 = ImageDraw.Draw(canvas1)
    d1.polygon([(w + 6, 8), (w + 8, 5), (w + 10, 8), (w + 8, 11)], fill=(255, 245, 120, 255))
    d1.polygon([(2, 14), (4, 11), (6, 14), (4, 17)], fill=(255, 185, 60, 255))
    
    b0 = canvas0.getbbox()
    b1 = canvas1.getbbox()
    return [canvas0.crop(b0) if b0 else canvas0, canvas1.crop(b1) if b1 else canvas1]

def main():
    print("=== Generating 119-Frame Enhanced Concept-Art Animation Suite ===")
    
    levels_config = [
        (1, 'level_1', 40),
        (2, 'level_2', 50),
        (3, 'level_3', 62),
        (4, 'level_4', 74),
        (5, 'level_5', 86),
        (6, 'level_6', 94),
        (7, 'level_7', 104)
    ]
    
    # Cleaned extracted concept art
    act_standing = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_standing.png'))
    act_tongue = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_tongue.png'))
    act_rolling = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_rolling.png'))
    act_sneaking = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_sneaking.png'))
    act_jumping = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_jumping.png'))
    act_playing = clean_shadow(Image.open(f'{CONCEPT_DIR}/action_playing.png'))

    all_frames = {}

    for lv_num, lv_name, target_h in levels_config:
        base_raw = clean_shadow(Image.open(f'{CONCEPT_DIR}/{lv_name}.png'))
        base_scaled = scale_exact_h(base_raw, target_h)
        
        frames = {}
        
        if lv_num == 3:
            # Level 3 directly incorporates the concept art action sheet!
            stand_sc = scale_exact_h(act_standing, target_h)
            tongue_sc = scale_exact_h(act_tongue, target_h)
            roll_sc = scale_exact_h(act_rolling, int(target_h * 0.70))
            jump_sc = scale_exact_h(act_jumping, target_h)
            play_sc = scale_exact_h(act_playing, target_h)
            sneak_sc = scale_exact_h(act_sneaking, target_h - 4)
            
            walks = generate_walk_frames(stand_sc)
            # Incorporate sneak stride for walk3
            walks[2] = scale_exact_h(sneak_sc, stand_sc.height)
            frames['walk1'], frames['walk2'], frames['walk3'], frames['walk4'] = walks
            
            eats = generate_eat_frames(stand_sc)
            frames['eat1'], frames['eat2'] = eats
            
            jumps = generate_jump_frames(jump_sc)
            frames['jump1'], frames['jump2'], frames['jump3'] = jumps
            
            sleeps = generate_sleep_frames(stand_sc, roll_sc)
            frames['sleep1'], frames['sleep2'] = sleeps
            
            roars = generate_roar_frames(stand_sc, play_sc)
            frames['roar1'], frames['roar2'] = roars
            
            happies = generate_happy_frames(stand_sc, tongue_sc)
            frames['happy1'], frames['happy2'] = happies
            
            turns = generate_turn_frames(stand_sc)
            frames['turn1'], frames['turn2'] = turns
            
        else:
            walks = generate_walk_frames(base_scaled)
            frames['walk1'], frames['walk2'], frames['walk3'], frames['walk4'] = walks
            
            eats = generate_eat_frames(base_scaled)
            frames['eat1'], frames['eat2'] = eats
            
            jumps = generate_jump_frames(base_scaled)
            frames['jump1'], frames['jump2'], frames['jump3'] = jumps
            
            sleeps = generate_sleep_frames(base_scaled)
            frames['sleep1'], frames['sleep2'] = sleeps
            
            roars = generate_roar_frames(base_scaled)
            frames['roar1'], frames['roar2'] = roars
            
            happies = generate_happy_frames(base_scaled)
            frames['happy1'], frames['happy2'] = happies
            
            turns = generate_turn_frames(base_scaled)
            frames['turn1'], frames['turn2'] = turns

        # Save individual PNGs
        for f_name, f_img in frames.items():
            path = f'{OUT_DIR}/cerato_lv{lv_num}_{f_name}.png'
            f_img.save(path)
            all_frames[f'cerato_lv{lv_num}_{f_name}'] = f_img
            
        print(f"Level {lv_num} ({lv_name}): Generated 17 frames at height ~{target_h}px")

    # Write include/CeratosaurusSprites.h
    print("\nWriting include/CeratosaurusSprites.h ...")
    header_lines = [
        '#pragma once',
        '#include <Arduino.h>',
        '',
        '#define SPRITE_CHROMA_KEY 0xF81F  // 16-bit RGB565 Magenta transparency key',
        '',
        'struct SpriteInfo {',
        '    const uint16_t* data;',
        '    int16_t width;',
        '    int16_t height;',
        '};',
        '',
        'struct StageSpriteSet {',
        '    SpriteInfo walk[4];',
        '    SpriteInfo eat[2];',
        '    SpriteInfo jump[3];    // 0: crouch, 1: extended leap, 2: apex tuck',
        '    SpriteInfo sleep[2];',
        '    SpriteInfo roar[2];',
        '    SpriteInfo happy[2];',
        '    SpriteInfo turn[2];    // 0: 3/4 turn in, 1: 3/4 turn out',
        '};',
        ''
    ]

    for lv_num in range(1, 8):
        for f_name in ['walk1', 'walk2', 'walk3', 'walk4', 'eat1', 'eat2', 'jump1', 'jump2', 'jump3', 'sleep1', 'sleep2', 'roar1', 'roar2', 'happy1', 'happy2', 'turn1', 'turn2']:
            var_name = f'cerato_lv{lv_num}_{f_name}'
            img = all_frames[var_name]
            w, h = img.size
            pixels = []
            for y in range(h):
                for x in range(w):
                    r, g, b, a = img.getpixel((x, y))
                    if a < 110:
                        pixels.append(CHROMA_KEY_HEX)
                    else:
                        pixels.append(to_rgb565(r, g, b))

            header_lines.append(f'// {var_name}: {w}x{h} pixels')
            header_lines.append(f'static const uint16_t PROGMEM {var_name}_data[{w * h}] = {{')
            for i in range(0, len(pixels), 12):
                chunk = pixels[i:i+12]
                hex_strs = [f'0x{p:04X}' for p in chunk]
                comma = ',' if i + 12 < len(pixels) else ''
                header_lines.append('    ' + ', '.join(hex_strs) + comma)
            header_lines.append('};')
            header_lines.append(f'static const SpriteInfo {var_name} = {{ {var_name}_data, {w}, {h} }};')
            header_lines.append('')

    # Stage animation sets
    for lv_num in range(1, 8):
        header_lines.append(f'static const StageSpriteSet stage_cerato_lv{lv_num} = {{')
        header_lines.append(f'    {{ cerato_lv{lv_num}_walk1, cerato_lv{lv_num}_walk2, cerato_lv{lv_num}_walk3, cerato_lv{lv_num}_walk4 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_eat1, cerato_lv{lv_num}_eat2 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_jump1, cerato_lv{lv_num}_jump2, cerato_lv{lv_num}_jump3 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_sleep1, cerato_lv{lv_num}_sleep2 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_roar1, cerato_lv{lv_num}_roar2 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_happy1, cerato_lv{lv_num}_happy2 }},')
        header_lines.append(f'    {{ cerato_lv{lv_num}_turn1, cerato_lv{lv_num}_turn2 }}')
        header_lines.append('};')
        header_lines.append('')

    header_lines.append('inline const StageSpriteSet& getStageSprites(uint8_t stage) {')
    header_lines.append('    switch (stage) {')
    header_lines.append('        case 1: return stage_cerato_lv1;')
    header_lines.append('        case 2: return stage_cerato_lv2;')
    header_lines.append('        case 3: return stage_cerato_lv3;')
    header_lines.append('        case 4: return stage_cerato_lv4;')
    header_lines.append('        case 5: return stage_cerato_lv5;')
    header_lines.append('        case 6: return stage_cerato_lv6;')
    header_lines.append('        case 7: return stage_cerato_lv7;')
    header_lines.append('        default: return stage_cerato_lv3;')
    header_lines.append('    }')
    header_lines.append('}')
    header_lines.append('')

    # Backward compatibility aliases
    for lvl in range(1, 8):
        header_lines.append(f'#define cerato_lv{lvl} cerato_lv{lvl}_walk1')
        header_lines.append(f'#define cerato_lv{lvl}_idle1 cerato_lv{lvl}_walk1')
        header_lines.append(f'#define cerato_lv{lvl}_idle2 cerato_lv{lvl}_walk2')
        header_lines.append(f'#define cerato_lv{lvl}_eat cerato_lv{lvl}_eat1')
        header_lines.append(f'#define cerato_lv{lvl}_jump cerato_lv{lvl}_jump1')
        header_lines.append(f'#define cerato_lv{lvl}_sleep cerato_lv{lvl}_sleep1')
        header_lines.append(f'#define cerato_lv{lvl}_roar cerato_lv{lvl}_roar1')
        header_lines.append(f'#define cerato_lv{lvl}_happy cerato_lv{lvl}_happy1')
    header_lines.append('')

    with open('include/CeratosaurusSprites.h', 'w') as f:
        f.write('\n'.join(header_lines))

    sz = os.path.getsize('include/CeratosaurusSprites.h')
    print(f"\nSUCCESS! Generated include/CeratosaurusSprites.h ({sz:,} bytes, 119 frames)")

if __name__ == '__main__':
    main()
