#!/usr/bin/env python3
"""
Generate a 430x430 speedometer dial face PNG matching the LVGL lv_meter geometry.

Meter parameters (from DashboardUI.cpp):
  - Size: 430x430, center at (215, 215)
  - Speed scale: 0-140 mph, 210 degree sweep, rotation start 165 degrees
  - LVGL angle convention: 0 = 12 o'clock, clockwise
  - Minor ticks: 141 (every 1 mph), 1px wide, 10px long, grey #808080
  - Major ticks: every 10th (0,10,20,...,140), 3px wide, 18px long, white
  - Major tick labels: Montserrat 20px, white, 16px gap inward from tick end
  - Background: black

Output: dial_face.png (430x430, RGB565-compatible)
        dial_face.c (LVGL C-array, CF_TRUE_COLOR format)
"""

import math
import struct
import sys
import os

from PIL import Image, ImageDraw, ImageFont

# --- Geometry constants ---
IMG_SIZE = 430
SSAA_SCALE = 4  # supersampling factor; render at 4x then downsample for anti-aliasing
_S = SSAA_SCALE
CENTER = (IMG_SIZE * _S) // 2
# The meter widget has padding/border that reduces the usable radius.
# LVGL lv_meter default inner padding is ~14px, so tick outer edge radius ~ 215-14 = 201
OUTER_RADIUS = 201 * _S

# Scale parameters
RANGE_MIN = 0
RANGE_MAX = 140
ANGLE_RANGE = 210  # degrees
ROTATION = 255     # starting angle in LVGL convention (0=12 o'clock, CW); shifted 90° CW from original 165°

# Tick parameters
MINOR_TICK_COUNT = 141  # one per mph (0 through 140 inclusive)
MINOR_TICK_WIDTH = 1 * _S
MINOR_TICK_LEN = 10 * _S
MINOR_TICK_COLOR = (0x80, 0x80, 0x80)

MAJOR_EVERY = 10  # every 10th minor tick
MAJOR_TICK_WIDTH = 3 * _S
MAJOR_TICK_LEN = 18 * _S
MAJOR_TICK_COLOR = (0xFF, 0xFF, 0xFF)

LABEL_GAP = 16 * _S  # pixels inward from inner end of major tick
LABEL_COLOR = (0xFF, 0xFF, 0xFF)
LABEL_FONT_SIZE = 20 * _S


def lvgl_angle_to_math(lvgl_deg):
    """Convert LVGL angle (0=12 o'clock, CW) to standard math angle (0=3 o'clock, CCW) in radians."""
    # LVGL: 0 = up (12 o'clock), increases clockwise
    # Math: 0 = right (3 o'clock), increases counter-clockwise
    # math_angle = 90 - lvgl_angle
    math_deg = 90.0 - lvgl_deg
    return math.radians(math_deg)


def tick_angle(index, total_ticks):
    """Return the LVGL angle (degrees) for tick at given index."""
    if total_ticks <= 1:
        return ROTATION
    frac = index / (total_ticks - 1)
    return ROTATION + frac * ANGLE_RANGE


def draw_tick(draw, angle_deg, outer_r, length, width, color):
    """Draw a radial tick line from outer_r inward by length pixels."""
    rad = lvgl_angle_to_math(angle_deg)
    cos_a = math.cos(rad)
    sin_a = math.sin(rad)

    x1 = CENTER + outer_r * cos_a
    y1 = CENTER - outer_r * sin_a
    inner_r = outer_r - length
    x2 = CENTER + inner_r * cos_a
    y2 = CENTER - inner_r * sin_a

    draw.line([(x1, y1), (x2, y2)], fill=color, width=width)


def find_font(size):
    """Try to find Montserrat font"""

    # Try to download
    try:
        import urllib.request
        import tempfile
        url = "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Medium.ttf"
        font_path = os.path.join(tempfile.gettempdir(), "Montserrat-Medium.ttf")
        if not os.path.exists(font_path):
            print(f"Downloading Montserrat font...")
            urllib.request.urlretrieve(url, font_path)
        return ImageFont.truetype(font_path, size)
    except Exception as e:
        print(f"Warning: Could not get Montserrat font ({e}), using default")
        return ImageFont.load_default()


def generate_png(output_path):
    """Generate the dial face PNG (rendered at SSAA_SCALE x resolution, downsampled)."""
    scaled_size = IMG_SIZE * SSAA_SCALE
    img = Image.new("RGB", (scaled_size, scaled_size), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    font = find_font(LABEL_FONT_SIZE)

    # Draw all ticks
    for i in range(MINOR_TICK_COUNT):
        angle = tick_angle(i, MINOR_TICK_COUNT)
        mph_value = RANGE_MIN + i * (RANGE_MAX - RANGE_MIN) / (MINOR_TICK_COUNT - 1)
        is_major = (round(mph_value) % MAJOR_EVERY == 0) and (abs(mph_value - round(mph_value)) < 0.01)

        if is_major:
            draw_tick(draw, angle, OUTER_RADIUS, MAJOR_TICK_LEN, MAJOR_TICK_WIDTH, MAJOR_TICK_COLOR)

            # Draw label
            label_text = str(int(round(mph_value)))
            rad = lvgl_angle_to_math(angle)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            label_r = OUTER_RADIUS - MAJOR_TICK_LEN - LABEL_GAP
            lx = CENTER + label_r * cos_a
            ly = CENTER - label_r * sin_a

            # Get text bounding box for centering
            bbox = font.getbbox(label_text)
            tw = bbox[2] - bbox[0]
            th = bbox[3] - bbox[1]
            draw.text((lx - tw / 2, ly - th / 2), label_text, fill=LABEL_COLOR, font=font)
        else:
            draw_tick(draw, angle, OUTER_RADIUS, MINOR_TICK_LEN, MINOR_TICK_WIDTH, MINOR_TICK_COLOR)

    img = img.resize((IMG_SIZE, IMG_SIZE), Image.LANCZOS)
    img.save(output_path)
    print(f"Generated {output_path} ({IMG_SIZE}x{IMG_SIZE})")
    return img


def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def generate_c_array(img, output_path):
    """Convert PIL image to LVGL CF_TRUE_COLOR (RGB565) C-array."""
    width, height = img.size
    pixels = img.load()
    data_size = width * height * 2  # 2 bytes per pixel (RGB565)

    # Build pixel data as bytes (little-endian RGB565)
    pixel_bytes = bytearray(data_size)
    idx = 0
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            pixel_bytes[idx] = rgb565 & 0xFF
            pixel_bytes[idx + 1] = (rgb565 >> 8) & 0xFF
            idx += 2

    # Write C file
    with open(output_path, "w") as f:
        f.write("/**\n")
        f.write(" * dial_face.c - Pre-rendered speedometer dial face (430x430 RGB565)\n")
        f.write(" *\n")
        f.write(" * Auto-generated by tools/generate_dial_face.py\n")
        f.write(" * DO NOT EDIT MANUALLY\n")
        f.write(" */\n\n")
        f.write("#include \"lvgl.h\"\n\n")
        f.write("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#endif\n\n")
        f.write("#ifndef LV_ATTRIBUTE_LARGE_CONST\n")
        f.write("#define LV_ATTRIBUTE_LARGE_CONST\n")
        f.write("#endif\n\n")

        f.write(f"static LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_MEM_ALIGN const uint8_t dial_face_map[] = {{\n")

        # Write in rows of 16 bytes
        for i in range(0, len(pixel_bytes), 16):
            chunk = pixel_bytes[i:i + 16]
            hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_vals},\n")

        f.write("};\n\n")

        f.write("const lv_img_dsc_t dial_face = {\n")
        f.write("    .header = {\n")
        f.write(f"        .cf = LV_IMG_CF_TRUE_COLOR,\n")
        f.write(f"        .always_zero = 0,\n")
        f.write(f"        .reserved = 0,\n")
        f.write(f"        .w = {width},\n")
        f.write(f"        .h = {height},\n")
        f.write("    },\n")
        f.write(f"    .data_size = {data_size},\n")
        f.write(f"    .data = dial_face_map,\n")
        f.write("};\n")

    print(f"Generated {output_path} ({data_size} bytes pixel data)")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dashboard_dir = os.path.join(script_dir, "..", "src", "DashboardUI")

    png_path = os.path.join(script_dir, "dial_face.png")
    c_path = os.path.join(dashboard_dir, "dial_face.c")

    img = generate_png(png_path)
    generate_c_array(img, c_path)
    print("Done!")


if __name__ == "__main__":
    main()
