#!/usr/bin/env python3
"""
Clay Render Log → PNG Screenshot Renderer

Reads JSONL render command logs from the Clay standalone test program
and produces PNG screenshots for each layout.

Usage:
    python3 render_clay_log.py <input.jsonl> <output_dir>

Output:
    One PNG per layout in the output directory.
"""

import json
import os
import sys
from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------------------
# Font setup
# ---------------------------------------------------------------------------

FONT_PATHS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
]

_font_cache = {}

def get_font(size):
    size = max(8, min(size, 72))
    if size in _font_cache:
        return _font_cache[size]
    for path in FONT_PATHS:
        if os.path.exists(path):
            try:
                font = ImageFont.truetype(path, size)
                _font_cache[size] = font
                return font
            except Exception:
                continue
    # Fallback to default
    font = ImageFont.load_default()
    _font_cache[size] = font
    return font

# ---------------------------------------------------------------------------
# Color helpers
# ---------------------------------------------------------------------------

def clay_color_to_rgba(c):
    """Convert Clay color array [r, g, b, a] (0-255) to PIL RGBA tuple."""
    r = max(0, min(255, int(c[0])))
    g = max(0, min(255, int(c[1])))
    b = max(0, min(255, int(c[2])))
    a = max(0, min(255, int(c[3])))
    return (r, g, b, a)

def alpha_blend(fg, bg):
    """Alpha-blend fg over bg (both RGBA tuples)."""
    if fg[3] == 0:
        return bg
    if fg[3] == 255:
        return (fg[0], fg[1], fg[2], 255)
    alpha = fg[3] / 255.0
    r = int(fg[0] * alpha + bg[0] * (1 - alpha))
    g = int(fg[1] * alpha + bg[1] * (1 - alpha))
    b = int(fg[2] * alpha + bg[2] * (1 - alpha))
    return (r, g, b, 255)

# ---------------------------------------------------------------------------
# Rounded rectangle helper
# ---------------------------------------------------------------------------

def draw_rounded_rect(draw, bbox, radius, fill):
    """Draw a filled rounded rectangle."""
    x0, y0, x1, y1 = bbox
    r = min(radius, (x1 - x0) // 2, (y1 - y0) // 2)
    if r <= 0:
        draw.rectangle(bbox, fill=fill)
        return
    # Center rect
    draw.rectangle([x0 + r, y0, x1 - r, y1], fill=fill)
    # Left rect
    draw.rectangle([x0, y0 + r, x0 + r, y1 - r], fill=fill)
    # Right rect
    draw.rectangle([x1 - r, y0 + r, x1, y1 - r], fill=fill)
    # Corners
    draw.pieslice([x0, y0, x0 + 2*r, y0 + 2*r], 180, 270, fill=fill)
    draw.pieslice([x1 - 2*r, y0, x1, y0 + 2*r], 270, 360, fill=fill)
    draw.pieslice([x0, y1 - 2*r, x0 + 2*r, y1], 90, 180, fill=fill)
    draw.pieslice([x1 - 2*r, y1 - 2*r, x1, y1], 0, 90, fill=fill)

# ---------------------------------------------------------------------------
# Renderer
# ---------------------------------------------------------------------------

def render_layout(frame_data, screen_w=1280, screen_h=720):
    """Render a single layout frame to a PIL Image."""
    # Create base image (dark background)
    img = Image.new('RGBA', (screen_w, screen_h), (20, 20, 30, 255))
    draw = ImageDraw.Draw(img)

    # Track clip rect (scissor) state
    clip_stack = []
    current_clip = None  # (x0, y0, x1, y1) or None

    commands = frame_data.get('commands', [])
    layout_name = frame_data.get('layout', 'unknown')

    for cmd in commands:
        cmd_type = cmd.get('type', '')
        x = cmd.get('x', 0)
        y = cmd.get('y', 0)
        w = cmd.get('w', 0)
        h = cmd.get('h', 0)

        if cmd_type == 'rectangle':
            bg = cmd.get('bg', [0, 0, 0, 0])
            color = clay_color_to_rgba(bg)
            # We can't do proper alpha blending with ImageDraw directly,
            # so use a temp layer for semi-transparent rects
            if color[3] < 255 and color[3] > 0:
                temp = Image.new('RGBA', (int(w), int(h)), color)
                img.paste(temp, (int(x), int(y)), temp)
            elif color[3] == 255:
                draw.rectangle([int(x), int(y), int(x + w), int(y + h)], fill=color)

        elif cmd_type == 'text':
            text = cmd.get('text', '')
            tc = cmd.get('textColor', [255, 255, 255, 255])
            font_size = cmd.get('fontSize', 16)
            color = clay_color_to_rgba(tc)
            try:
                font = get_font(font_size)
                draw.text((int(x), int(y)), text, fill=color, font=font)
            except Exception:
                draw.text((int(x), int(y)), text, fill=color)

        elif cmd_type == 'border':
            bc = cmd.get('borderColor', [255, 255, 255, 255])
            bw = cmd.get('borderWidth', {})
            color = clay_color_to_rgba(bc)
            left = int(x)
            top = int(y)
            right = int(x + w)
            bottom = int(y + h)

            bl = bw.get('l', 0)
            br = bw.get('r', 0)
            bt = bw.get('t', 0)
            bb = bw.get('b', 0)

            if bt:
                for i in range(bt):
                    draw.line([(left, top + i), (right, top + i)], fill=color, width=1)
            if bb:
                for i in range(bb):
                    draw.line([(left, bottom - 1 - i), (right, bottom - 1 - i)], fill=color, width=1)
            if bl:
                for i in range(bl):
                    draw.line([(left + i, top), (left + i, bottom)], fill=color, width=1)
            if br:
                for i in range(br):
                    draw.line([(right - 1 - i, top), (right - 1 - i, bottom)], fill=color, width=1)

        elif cmd_type == 'image':
            # Draw magenta placeholder with X
            draw.rectangle([int(x), int(y), int(x + w), int(y + h)],
                         fill=(255, 0, 255, 255))
            draw.line([(int(x), int(y)), (int(x + w), int(y + h))],
                     fill=(0, 0, 0, 255), width=2)
            draw.line([(int(x + w), int(y)), (int(x), int(y + h))],
                     fill=(0, 0, 0, 255), width=2)

        elif cmd_type == 'custom':
            cbg = cmd.get('customBg', [0, 0, 0, 0])
            color = clay_color_to_rgba(cbg)
            if color[3] > 0:
                draw.rectangle([int(x), int(y), int(x + w), int(y + h)], fill=color)
            else:
                # Outline for visibility
                draw.rectangle([int(x), int(y), int(x + w), int(y + h)],
                             outline=(128, 128, 128, 80), width=1)

        elif cmd_type == 'scissor_start':
            clip_stack.append(current_clip)
            current_clip = (int(x), int(y), int(x + w), int(y + h))

        elif cmd_type == 'scissor_end':
            if clip_stack:
                current_clip = clip_stack.pop()
            else:
                current_clip = None

    # Add layout name watermark
    watermark = f"Layout: {layout_name} | {screen_w}x{screen_h}"
    draw.text((8, screen_h - 20), watermark, fill=(180, 180, 180, 180), font=get_font(12))

    return img

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.jsonl> <output_dir>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    os.makedirs(output_dir, exist_ok=True)

    with open(input_file, 'r') as f:
        lines = f.readlines()

    screenshots = []

    for line_num, line in enumerate(lines):
        line = line.strip()
        if not line:
            continue
        try:
            frame_data = json.loads(line)
        except json.JSONDecodeError as e:
            print(f"Warning: skipping invalid JSON on line {line_num + 1}: {e}")
            continue

        layout_name = frame_data.get('layout', f'frame_{line_num}')
        num_commands = len(frame_data.get('commands', []))

        print(f"Rendering layout '{layout_name}' ({num_commands} commands)...")

        img = render_layout(frame_data)

        out_path = os.path.join(output_dir, f"{layout_name}.png")
        img.save(out_path, 'PNG')
        screenshots.append(out_path)
        print(f"  Saved: {out_path} ({img.size[0]}x{img.size[1]})")

    print(f"\nRendered {len(screenshots)} layout screenshots to {output_dir}/")

    # Summary image
    if len(screenshots) > 1:
        thumb_w = 426
        thumb_h = 240
        cols = min(3, len(screenshots))
        rows = (len(screenshots) + cols - 1) // cols
        summary = Image.new('RGBA', (cols * thumb_w + 20, rows * thumb_h + 40),
                          (30, 30, 40, 255))
        summary_draw = ImageDraw.Draw(summary)
        summary_draw.text((10, 5), "Clay UI Test - All Layouts", fill=(255, 255, 255, 255),
                         font=get_font(18))

        for i, path in enumerate(screenshots):
            thumb = Image.open(path).resize((thumb_w, thumb_h), Image.LANCZOS)
            col = i % cols
            row = i // cols
            px = col * thumb_w + 10
            py = row * thumb_h + 30
            summary.paste(thumb, (px, py))

        summary_path = os.path.join(output_dir, "all_layouts_summary.png")
        summary.save(summary_path, 'PNG')
        print(f"Summary image saved: {summary_path}")

if __name__ == '__main__':
    main()
