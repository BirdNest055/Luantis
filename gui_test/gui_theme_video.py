#!/usr/bin/env python3
"""
Luantis GUITheme — 30-Second Animated Video
Shows changing GUI elements with the centralized GUITheme system.

Renders 900 frames at 30fps = 30 seconds.
Sections:
  0-5s:  Title + GUITheme overview
  5-10s: Color palette with animated highlights
  10-15s: Pause menu with button hover/press states
  15-20s: Inventory slots with hover animation
  20-25s: Theme switching (old hardcoded → new centralized)
  25-30s: Summary + fade out
"""

import os, sys, math, json

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, Rectangle, Circle
import matplotlib.font_manager as fm

fm.fontManager.addfont('/usr/share/fonts/truetype/chinese/SarasaMonoSC-Regular.ttf')
fm.fontManager.addfont('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
plt.rcParams['font.sans-serif'] = ['Sarasa Mono SC', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

OUTPUT_DIR = "/tmp/gui_theme_video_frames"
os.makedirs(OUTPUT_DIR, exist_ok=True)

FPS = 30
DURATION = 30
TOTAL_FRAMES = FPS * DURATION
W, H = 1280, 720
DPI = 100

# Theme constants (mirrored from GUITheme.h)
class Theme:
    class Colors:
        MODAL_BG = (0, 0, 0, 140/255)
        MODAL_BG_FULLSCREEN = (0, 0, 0, 1.0)
        TOOLTIP_BG = (60/255, 130/255, 110/255, 1.0)
        TOOLTIP_TEXT = (1, 1, 1, 1)
        TEXT_DEFAULT = (1, 1, 1, 1)
        BUTTON_BG_DEFAULT = (1, 1, 1, 1)
        BUTTON_TEXT_DEFAULT = (1, 1, 1, 1)
        BUTTON_OVERRIDE_DEFAULT = (1, 1, 1, 101/255)
        SLOT_BORDER = (0, 0, 0, 200/255)
        SLOT_BG_NORMAL = (0.5, 0.5, 0.5, 1)
        SLOT_BG_HOVERED = (0.75, 0.75, 0.75, 1)
        TABLE_TEXT_DEFAULT = (1, 1, 1, 1)
        TABLE_BG_DEFAULT = (0, 0, 0, 1)
        TABLE_HIGHLIGHT = (50/255, 100/255, 70/255, 1)
        STATUS_TEXT_MAIN_BG = (0, 0, 0, 220/255)
        CHAT_CONSOLE_BG = (0, 0, 0, 1)
        CHAT_CONSOLE_CURSOR = (1, 1, 1, 1)
        CHAT_CONSOLE_TEXT = (1, 1, 1, 1)
        TOUCH_SELECTION = (0.5, 0.5, 0.5, 1)
        TOUCH_ERROR = (1, 0, 0, 1)
    class ButtonModifiers:
        HOVER_BRIGHTEN = 1.25
        PRESS_DARKEN = 0.85

# Helper: ease-in-out
def ease(t):
    return t * t * (3 - 2 * t)

# Helper: lerp
def lerp(a, b, t):
    return a + (b - a) * t

def lerp_color(c1, c2, t):
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))

def brighten(color, factor):
    rgb = list(color[:3])
    return (min(1, rgb[0]*factor), min(1, rgb[1]*factor), min(1, rgb[2]*factor), color[3])

# ============================================================
# Section 0: Title (frames 0-149, 0-5s)
# ============================================================
def render_title(frame, ax):
    t = frame / 150.0  # 0..1

    ax.set_facecolor('#1a1a2e')
    fig = ax.figure
    fig.patch.set_facecolor('#1a1a2e')

    # Animated title
    alpha = min(1.0, t * 3)
    scale = ease(min(1.0, t * 2))

    ax.text(0.5, 0.72, "GUITheme", ha='center', va='center',
            fontsize=48 * scale, color='#6aaa6a', fontweight='bold',
            alpha=alpha, transform=ax.transAxes)
    ax.text(0.5, 0.58, "Centralized GUI Theme System", ha='center', va='center',
            fontsize=18 * scale, color='white', alpha=alpha * 0.8,
            transform=ax.transAxes)
    ax.text(0.5, 0.48, "v9.50 — Based on clawtest-v9.44-voice-server-authority",
            ha='center', va='center', fontsize=12, color='#888',
            alpha=alpha * 0.7, transform=ax.transAxes)

    # Floating color dots animation
    if t > 0.3:
        dot_t = (t - 0.3) / 0.7
        colors_rgb = [
            (0.4, 0.7, 0.4), (0.7, 0.5, 0.2), (0.5, 0.5, 0.8),
            (0.8, 0.3, 0.3), (0.3, 0.7, 0.7), (0.7, 0.7, 0.3),
        ]
        for i, (cr, cg, cb) in enumerate(colors_rgb):
            angle = dot_t * math.pi * 2 + i * math.pi / 3
            x = 0.5 + 0.25 * math.cos(angle) * dot_t
            y = 0.25 + 0.08 * math.sin(angle * 1.5) * dot_t
            r = 0.02 + 0.01 * math.sin(angle * 2)
            circle = Circle((x, y), r, facecolor=(cr, cg, cb),
                           alpha=min(1.0, dot_t * 2), edgecolor='white', linewidth=0.5)
            ax.add_patch(circle)

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')

# ============================================================
# Section 1: Color palette (frames 150-299, 5-10s)
# ============================================================
def render_color_palette(frame, ax):
    local_frame = frame - 150
    t = local_frame / 150.0  # 0..1

    ax.set_facecolor('#2b2b2b')
    fig = ax.figure
    fig.patch.set_facecolor('#2b2b2b')

    ax.text(0.5, 0.96, "GUITheme::Colors — 22 Color Constants", ha='center', va='top',
            fontsize=16, color='white', fontweight='bold', transform=ax.transAxes)

    colors = [
        ("MODAL_BG", Theme.Colors.MODAL_BG, "Overlay bg"),
        ("MODAL_BG_FS", Theme.Colors.MODAL_BG_FULLSCREEN, "Fullscreen bg"),
        ("TOOLTIP_BG", Theme.Colors.TOOLTIP_BG, "Tooltip bg"),
        ("TOOLTIP_TEXT", Theme.Colors.TOOLTIP_TEXT, "Tooltip text"),
        ("TEXT_DEFAULT", Theme.Colors.TEXT_DEFAULT, "Default text"),
        ("BUTTON_BG", Theme.Colors.BUTTON_BG_DEFAULT, "Button bg"),
        ("BUTTON_OVERRIDE", Theme.Colors.BUTTON_OVERRIDE_DEFAULT, "Pressed blend"),
        ("SLOT_BORDER", Theme.Colors.SLOT_BORDER, "Slot border"),
        ("SLOT_BG_NORMAL", Theme.Colors.SLOT_BG_NORMAL, "Slot normal"),
        ("SLOT_BG_HOVERED", Theme.Colors.SLOT_BG_HOVERED, "Slot hovered"),
        ("TABLE_HIGHLIGHT", Theme.Colors.TABLE_HIGHLIGHT, "Row highlight"),
        ("CHAT_CONSOLE_BG", Theme.Colors.CHAT_CONSOLE_BG, "Chat bg"),
        ("TOUCH_ERROR", Theme.Colors.TOUCH_ERROR, "Touch error"),
        ("TOUCH_SELECTION", Theme.Colors.TOUCH_SELECTION, "Touch select"),
    ]

    cols = 4
    rows = (len(colors) + cols - 1) // cols
    sw, sh, gx, gy = 0.2, 0.07, 0.24, 0.14

    # Which item to highlight (cycle through them)
    highlight_idx = int(t * len(colors)) % len(colors)

    for i, (name, color, desc) in enumerate(colors):
        r, c = i // cols, i % cols
        x, y = 0.04 + c * gx, 0.85 - r * gy

        # Staggered appearance
        appear_t = max(0, min(1, (t - i * 0.04) * 4))
        if appear_t <= 0:
            continue

        is_highlight = (i == highlight_idx and t > 0.2)
        alpha_mult = 1.0 if not is_highlight else 1.0 + 0.3 * math.sin(t * 20)
        edge_color = '#ffff00' if is_highlight else 'white'
        edge_width = 2.5 if is_highlight else 0.5

        rect = FancyBboxPatch((x, y - sh), sw, sh, boxstyle="round,pad=0.01",
            facecolor=color[:3], alpha=min(1.0, color[3] * appear_t * alpha_mult),
            edgecolor=edge_color, linewidth=edge_width)
        ax.add_patch(rect)

        ax.text(x + sw/2, y - sh - 0.015, name, ha='center', va='top',
                fontsize=6.5, color='white', fontweight='bold', alpha=appear_t)
        ax.text(x + sw/2, y - sh - 0.035, desc, ha='center', va='top',
                fontsize=5, color='#aaa', alpha=appear_t * 0.8)

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')

# ============================================================
# Section 2: Pause menu with button states (frames 300-449, 10-15s)
# ============================================================
def render_pause_menu(frame, ax):
    local_frame = frame - 300
    t = local_frame / 150.0  # 0..1

    ax.set_facecolor('#333')
    fig = ax.figure
    fig.patch.set_facecolor('#333')

    # Dark overlay
    ax.add_patch(Rectangle((0, 0), 1, 1, facecolor=Theme.Colors.MODAL_BG[:3],
                            alpha=Theme.Colors.MODAL_BG[3]))

    # Panel
    pw, ph = 0.5, 0.75
    px, py = 0.25, 0.12
    ax.add_patch(FancyBboxPatch((px, py), pw, ph, boxstyle="round,pad=0.02",
        facecolor='#1a1a2e', alpha=0.95, edgecolor='#4a4a6a', linewidth=2))

    ax.text(0.5, py + ph - 0.06, "Game Paused", ha='center', va='top',
            fontsize=20, color=Theme.Colors.TEXT_DEFAULT[:3], fontweight='bold')

    buttons = ["Continue", "Settings", "Change Password", "Sound Volume", "Exit to Menu", "Exit to OS"]
    bw, bh = 0.38, 0.065

    # Cycle through buttons showing hover/press states
    cycle_period = 0.6  # seconds per button
    btn_idx = int((t * len(buttons)) / cycle_period) % len(buttons)
    # Determine state: hover then press
    btn_phase = (t * len(buttons) / cycle_period) % 1.0
    is_pressed = btn_phase > 0.6

    for i, label in enumerate(buttons):
        y = py + ph - 0.17 - i * 0.09
        bx = 0.5 - bw / 2

        # Base button color
        btn_color = Theme.Colors.BUTTON_BG_DEFAULT[:3]
        btn_alpha = 0.3
        text_color = Theme.Colors.TEXT_DEFAULT[:3]

        if i == btn_idx:
            if is_pressed:
                btn_color = brighten(Theme.Colors.BUTTON_BG_DEFAULT[:3] + (1,), Theme.ButtonModifiers.PRESS_DARKEN)[:3]
                btn_alpha = 0.6
                # Show pressed state indicator
                ax.text(0.5, y + bh + 0.01, f"PRESSED x{Theme.ButtonModifiers.PRESS_DARKEN}",
                        ha='center', va='bottom', fontsize=6, color='#ff8888', fontstyle='italic')
            else:
                btn_color = brighten(Theme.Colors.BUTTON_BG_DEFAULT[:3] + (1,), Theme.ButtonModifiers.HOVER_BRIGHTEN)[:3]
                btn_alpha = 0.5
                ax.text(0.5, y + bh + 0.01, f"HOVERED x{Theme.ButtonModifiers.HOVER_BRIGHTEN}",
                        ha='center', va='bottom', fontsize=6, color='#88ff88', fontstyle='italic')

        ax.add_patch(FancyBboxPatch((bx, y), bw, bh, boxstyle="round,pad=0.005",
            facecolor=btn_color, alpha=btn_alpha, edgecolor='#6a6a8a', linewidth=1))
        ax.text(0.5, y + bh / 2, label, ha='center', va='center',
                fontsize=11, color=text_color)

    # Labels
    ax.text(0.5, 0.05, "GUITheme::Colors::MODAL_BG + ButtonModifiers",
            ha='center', va='bottom', fontsize=10, color='#6aaa6a', fontweight='bold')

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')

# ============================================================
# Section 3: Inventory slots with hover animation (frames 450-599, 15-20s)
# ============================================================
def render_inventory(frame, ax):
    local_frame = frame - 450
    t = local_frame / 150.0  # 0..1

    ax.set_facecolor('#333')
    fig = ax.figure
    fig.patch.set_facecolor('#333')

    ax.text(0.5, 0.96, "Inventory Slots — GUITheme::Colors::SLOT_*",
            ha='center', va='top', fontsize=14, color='white', fontweight='bold')

    ss, gap = 0.08, 0.02
    total_cols = 9
    total_rows = 3

    # Animate a "cursor" moving across slots
    cursor_col = int(t * total_cols * 2) % total_cols
    cursor_row = int(t * total_cols * 2 / total_cols) % total_rows
    cursor_phase = (t * total_cols * 2) % 1.0

    for row in range(total_rows):
        for col in range(total_cols):
            x = 0.1 + col * (ss + gap)
            y = 0.82 - row * (ss + gap)

            is_hover = (row == cursor_row and col == cursor_col)
            dist = math.sqrt((row - cursor_row)**2 + (col - cursor_col)**2)

            if is_hover:
                bg = Theme.Colors.SLOT_BG_HOVERED
                border_alpha = 1.0
                label = "HOVER"
            else:
                # Subtle proximity effect
                proximity = max(0, 1 - dist * 0.3)
                bg = lerp_color(Theme.Colors.SLOT_BG_NORMAL, Theme.Colors.SLOT_BG_HOVERED, proximity * 0.3)
                border_alpha = Theme.Colors.SLOT_BORDER[3]
                label = ""

            ax.add_patch(FancyBboxPatch((x, y - ss), ss, ss, boxstyle="round,pad=0.002",
                facecolor=bg[:3], alpha=bg[3],
                edgecolor=Theme.Colors.SLOT_BORDER[:3], linewidth=border_alpha * 2))

            if label:
                ax.text(x + ss/2, y - ss/2, label, ha='center', va='center',
                        fontsize=5.5, color='black', fontweight='bold')

    # Show the color values being used
    info_y = 0.38
    ax.text(0.1, info_y, "SLOT_BG_NORMAL = SColor(255,128,128,128)",
            fontsize=9, color='#aaa', family='monospace')
    ax.text(0.1, info_y - 0.04, "SLOT_BG_HOVERED = SColor(255,192,192,192)",
            fontsize=9, color='#ccc', family='monospace')
    ax.text(0.1, info_y - 0.08, "SLOT_BORDER = SColor(200,0,0,0)",
            fontsize=9, color='#888', family='monospace')

    # Animate a mini diagram showing the slot size change
    ax.text(0.5, 0.18, "Sizing Constants", ha='center', fontsize=11, color='#6aaa6a', fontweight='bold')
    sizing_items = [
        f"SLOT_SPACING_RATIO = {0.25}",
        f"PADDING_RATIO = {0.05}",
        f"BUTTON_HEIGHT_RATIO = {15/13*0.35:.4f}",
    ]
    for i, item in enumerate(sizing_items):
        alpha = min(1.0, t * 3 - i * 0.3)
        ax.text(0.5, 0.13 - i * 0.035, item, ha='center', fontsize=9,
                color='#ccc', family='monospace', alpha=max(0, alpha))

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')

# ============================================================
# Section 4: Theme switching before/after (frames 600-749, 20-25s)
# ============================================================
def render_theme_switch(frame, ax):
    local_frame = frame - 600
    t = local_frame / 150.0  # 0..1

    fig = ax.figure
    fig.patch.set_facecolor('#1a1a2e')
    ax.set_facecolor('#1a1a2e')

    # Split screen: left = old hardcoded, right = new centralized
    split = 0.5

    # Transition: old fades out, new fades in
    transition = ease(max(0, min(1, (t - 0.15) / 0.5)))

    # LEFT: "Before" — hardcoded values scattered across files
    ax.add_patch(Rectangle((0, 0), split, 1, facecolor='#2a2020', alpha=0.8))
    ax.text(split * 0.5, 0.95, "BEFORE", ha='center', va='top',
            fontsize=18, color='#ff6666', fontweight='bold',
            alpha=1 - transition * 0.5)
    ax.text(split * 0.5, 0.89, "Hardcoded values in 14 files", ha='center', va='top',
            fontsize=9, color='#aa6666', alpha=1 - transition * 0.5)

    # Show scattered code snippets (animated)
    old_snippets = [
        ('guiFormSpecMenu.cpp', 'SColor(140, 0, 0, 0)'),
        ('guiButton.cpp', 'COLOR_HOVERED_MOD = 1.25'),
        ('guiPasswordChange.cpp', 'SColor(255, 255, 255, 255)'),
        ('guiVolumeChange.cpp', 'SColor(140, 0, 0, 0)'),
        ('guiChatConsole.h', 'SColor(255, 0, 0, 0)'),
        ('touchcontrols.cpp', 'SColor(255, 128, 128, 128)'),
    ]
    for i, (file, code) in enumerate(old_snippets):
        y = 0.78 - i * 0.09
        appear = min(1, t * 4 - i * 0.1)
        # Red strike-through effect when transitioning
        alpha = max(0, appear * (1 - transition))
        ax.text(0.03, y, file, fontsize=7, color='#ff8888', family='monospace', alpha=alpha)
        ax.text(0.03, y - 0.03, code, fontsize=6, color='#cc6666', family='monospace', alpha=alpha * 0.8)
        if transition > 0.3:
            ax.plot([0.02, 0.45], [y - 0.015, y - 0.015], color='red',
                    linewidth=1.5, alpha=transition * alpha)

    # RIGHT: "After" — centralized GUITheme
    ax.add_patch(Rectangle((split, 0), split, 1, facecolor='#202a20', alpha=0.8))
    ax.text(split + split * 0.5, 0.95, "AFTER", ha='center', va='top',
            fontsize=18, color='#66ff66', fontweight='bold',
            alpha=0.5 + transition * 0.5)
    ax.text(split + split * 0.5, 0.89, "GUITheme.h — Single source of truth", ha='center', va='top',
            fontsize=9, color='#66aa66', alpha=0.5 + transition * 0.5)

    new_snippets = [
        ('GUITheme.h', 'namespace Colors {'),
        ('GUITheme.h', '  MODAL_BG = SColor(140,0,0,0)'),
        ('GUITheme.h', '  SLOT_BG_NORMAL = SColor(255,128,128,128)'),
        ('GUITheme.h', 'namespace ButtonModifiers {'),
        ('GUITheme.h', '  HOVER_BRIGHTEN = 1.25'),
        ('GUITheme.h', 'inline bool validate() { ... }'),
    ]
    for i, (file, code) in enumerate(new_snippets):
        y = 0.78 - i * 0.09
        appear = max(0, min(1, (transition - 0.1) * 4 - i * 0.08))
        ax.text(split + 0.03, y, file, fontsize=7, color='#88ff88', family='monospace', alpha=appear)
        ax.text(split + 0.03, y - 0.03, code, fontsize=6, color='#66cc66', family='monospace', alpha=appear * 0.8)

    # Arrow in the middle
    arrow_alpha = max(0, min(1, transition * 3))
    ax.annotate('', xy=(0.58, 0.5), xytext=(0.42, 0.5),
                arrowprops=dict(arrowstyle='->', color='white', lw=3),
                alpha=arrow_alpha)

    # Stats at bottom
    if transition > 0.5:
        stats_alpha = min(1, (transition - 0.5) * 4)
        ax.text(0.5, 0.12, "14 files refactored | 22 color constants | 12 sizing constants | 8 test suites | 89 tests",
                ha='center', fontsize=9, color='#aaa', family='monospace', alpha=stats_alpha)
        ax.text(0.5, 0.06, "SColor(140,0,0,0) found 0 times outside GUITheme.h",
                ha='center', fontsize=8, color='#66ff66', family='monospace', alpha=stats_alpha)

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')

# ============================================================
# Section 5: Summary + fade out (frames 750-899, 25-30s)
# ============================================================
def render_summary(frame, ax):
    local_frame = frame - 750
    t = local_frame / 150.0  # 0..1

    fig = ax.figure
    fig.patch.set_facecolor('#1a1a2e')
    ax.set_facecolor('#1a1a2e')

    # Title
    alpha_title = min(1, t * 3)
    ax.text(0.5, 0.92, "GUITheme v9.50 — Complete Theme Summary",
            ha='center', va='top', fontsize=18, color='white',
            fontweight='bold', alpha=alpha_title)
    ax.text(0.5, 0.87, "src/gui/GUITheme.h + GUITheme.cpp",
            ha='center', va='top', fontsize=11, color='#6aaa6a', alpha=alpha_title * 0.8)

    # Category blocks
    categories = [
        ("Colors (22)", [
            "MODAL_BG, MODAL_BG_FULLSCREEN, TOOLTIP_BG, TOOLTIP_TEXT",
            "TEXT_DEFAULT, BUTTON_BG_DEFAULT, BUTTON_OVERRIDE_DEFAULT",
            "SLOT_BORDER, SLOT_BG_NORMAL, SLOT_BG_HOVERED",
            "TABLE_HIGHLIGHT, STATUS_TEXT_MAIN_BG, CHAT_CONSOLE_*",
            "TOUCH_SELECTION, TOUCH_ERROR",
        ], '#4a8a4a'),
        ("Sizing (12)", [
            "BUTTON_HEIGHT_RATIO = 15/13 * 0.35",
            "SLOT_SPACING_RATIO = 0.25  PADDING_RATIO = 0.05",
            "LOCK_SIZE = 800x600  TOOLTIP = 110x18",
            "STATUS_BAR_HEIGHT = 40  FORM_FALLBACK = 580x300",
        ], '#4a6a8a'),
        ("Timing (2)", [
            "STATUS_TEXT_DURATION_GAME = 1.5s",
            "STATUS_TEXT_DURATION_MENU = 3.0s",
        ], '#8a6a4a'),
        ("ButtonModifiers (2)", [
            "HOVER_BRIGHTEN = 1.25 (brightens 25%)",
            "PRESS_DARKEN = 0.85 (darkens 15%)",
        ], '#8a4a6a'),
    ]

    y_offset = 0.78
    for cat_name, items, cat_color in categories:
        appear = min(1, t * 5 - categories.index((cat_name, items, cat_color)) * 0.5)
        if appear <= 0:
            continue

        ax.text(0.08, y_offset, cat_name, fontsize=12, color=cat_color,
                fontweight='bold', family='monospace', alpha=appear)

        # Background bar
        bar_width = len(cat_name) * 0.012
        ax.add_patch(Rectangle((0.06, y_offset - 0.005), bar_width, 0.025,
                               facecolor=cat_color, alpha=appear * 0.2))

        for j, item in enumerate(items):
            ax.text(0.1, y_offset - 0.035 - j * 0.03, item,
                    fontsize=7, color='#ccc', family='monospace', alpha=appear * 0.9)

        y_offset -= 0.035 + len(items) * 0.03 + 0.04

    # Test results
    if t > 0.5:
        test_alpha = min(1, (t - 0.5) * 4)
        ax.text(0.5, 0.08, "89/89 TESTS PASSED | 8 TEST SUITES | validate() OK",
                ha='center', fontsize=12, color='#66ff66', fontweight='bold',
                alpha=test_alpha)

    # Fade out at the end
    if t > 0.85:
        fade = (t - 0.85) / 0.15
        ax.add_patch(Rectangle((0, 0), 1, 1, facecolor='black', alpha=fade))

    ax.set_xlim(0, 1); ax.set_ylim(0, 1); ax.axis('off')


def render_frame(frame):
    fig, ax = plt.subplots(figsize=(W/DPI, H/DPI), dpi=DPI)
    fig.subplots_adjust(0, 0, 1, 1)

    if frame < 150:
        render_title(frame, ax)
    elif frame < 300:
        render_color_palette(frame, ax)
    elif frame < 450:
        render_pause_menu(frame, ax)
    elif frame < 600:
        render_inventory(frame, ax)
    elif frame < 750:
        render_theme_switch(frame, ax)
    else:
        render_summary(frame, ax)

    path = os.path.join(OUTPUT_DIR, f"frame_{frame:04d}.png")
    fig.savefig(path, dpi=DPI, bbox_inches='tight', pad_inches=0,
                facecolor=fig.get_facecolor())
    plt.close(fig)
    return path


def main():
    print("=" * 60)
    print("  Luantis GUITheme — 30s Animated Video")
    print(f"  {TOTAL_FRAMES} frames at {FPS}fps = {DURATION}s")
    print("=" * 60)

    for frame in range(TOTAL_FRAMES):
        render_frame(frame)
        if frame % 30 == 0:
            pct = frame / TOTAL_FRAMES * 100
            print(f"  Rendered {frame}/{TOTAL_FRAMES} ({pct:.0f}%)")

    print(f"\nAll {TOTAL_FRAMES} frames rendered to {OUTPUT_DIR}/")
    print("Next: compile with ffmpeg")
    return 0


if __name__ == "__main__":
    sys.exit(main())
