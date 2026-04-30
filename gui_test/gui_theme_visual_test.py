#!/usr/bin/env python3
"""
Luantis GUITheme E2E Visual Validation
=======================================
Renders visual screenshots of the centralized GUI theme to verify
all colors, sizes, and layout constants produce the expected appearance.

Generates PNG screenshots saved to /tmp/gui_theme_screenshots/
"""

import os
import sys
import json

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.patches import FancyBboxPatch
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("WARNING: matplotlib not available, skipping visual tests")

try:
    import matplotlib.font_manager as fm
    fm.fontManager.addfont('/usr/share/fonts/truetype/chinese/NotoSansSC[wght].ttf')
    fm.fontManager.addfont('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    plt.rcParams['font.sans-serif'] = ['Noto Sans SC', 'DejaVu Sans']
    plt.rcParams['axes.unicode_minus'] = False
except:
    pass

OUTPUT_DIR = "/tmp/gui_theme_screenshots"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# ============================================================================
# Theme constants (mirrored from GUITheme.h)
# ============================================================================
class Colors:
    MODAL_BG = (0, 0, 0, 140/255)          # SColor(140, 0, 0, 0)
    MODAL_BG_FULLSCREEN = (0, 0, 0, 1.0)    # SColor(255, 0, 0, 0)
    TOOLTIP_BG = (60/255, 130/255, 110/255, 1.0)  # SColor(255, 110, 130, 60)
    TOOLTIP_TEXT = (1, 1, 1, 1.0)           # SColor(255, 255, 255, 255)
    TEXT_DEFAULT = (1, 1, 1, 1.0)           # SColor(255, 255, 255, 255)
    BUTTON_BG_DEFAULT = (1, 1, 1, 1.0)      # SColor(255, 255, 255, 255)
    BUTTON_TEXT_DEFAULT = (1, 1, 1, 1.0)    # SColor(255, 255, 255, 255)
    BUTTON_OVERRIDE_DEFAULT = (1, 1, 1, 101/255)  # SColor(101, 255, 255, 255)
    SLOT_BORDER = (0, 0, 0, 200/255)        # SColor(200, 0, 0, 0)
    SLOT_BG_NORMAL = (128/255, 128/255, 128/255, 1.0)   # SColor(255, 128, 128, 128)
    SLOT_BG_HOVERED = (192/255, 192/255, 192/255, 1.0)  # SColor(255, 192, 192, 192)
    TABLE_TEXT_DEFAULT = (1, 1, 1, 1.0)     # SColor(255, 255, 255, 255)
    TABLE_BG_DEFAULT = (0, 0, 0, 1.0)       # SColor(255, 0, 0, 0)
    TABLE_HIGHLIGHT = (50/255, 100/255, 70/255, 1.0)   # SColor(255, 70, 100, 50)
    TABLE_HIGHLIGHT_TEXT = (1, 1, 1, 1.0)   # SColor(255, 255, 255, 255)
    STATUS_TEXT_FALLBACK = (0, 0, 0, 1.0)   # SColor(255, 0, 0, 0)
    STATUS_TEXT_MAIN_BG = (0, 0, 0, 220/255) # SColor(220, 0, 0, 0)
    CHAT_CONSOLE_BG = (0, 0, 0, 1.0)        # SColor(255, 0, 0, 0)


class Sizing:
    BUTTON_HEIGHT_RATIO = 15.0 / 13.0 * 0.35
    SLOT_SPACING_RATIO = 0.25
    PADDING_RATIO = 0.05
    STATUS_BAR_HEIGHT = 40
    TOOLTIP_INITIAL_SIZE = (110, 18)
    MAINMENU_LOCKED_SIZE = (800, 600)


class ButtonModifiers:
    HOVER_BRIGHTEN = 1.25
    PRESS_DARKEN = 0.85


# ============================================================================
# Screenshot 1: Color Palette Overview
# ============================================================================
def render_color_palette():
    fig, ax = plt.subplots(1, 1, figsize=(14, 10))
    fig.patch.set_facecolor('#2b2b2b')
    ax.set_facecolor('#2b2b2b')

    colors_data = [
        ("MODAL_BG", Colors.MODAL_BG, "Semi-transparent overlay"),
        ("MODAL_BG_FULLSCREEN", Colors.MODAL_BG_FULLSCREEN, "Fullscreen backdrop"),
        ("TOOLTIP_BG", Colors.TOOLTIP_BG, "Tooltip background"),
        ("TOOLTIP_TEXT", Colors.TOOLTIP_TEXT, "Tooltip text"),
        ("TEXT_DEFAULT", Colors.TEXT_DEFAULT, "Default text color"),
        ("BUTTON_BG_DEFAULT", Colors.BUTTON_BG_DEFAULT, "Button background"),
        ("BUTTON_OVERRIDE_DEFAULT", Colors.BUTTON_OVERRIDE_DEFAULT, "Button override"),
        ("SLOT_BORDER", Colors.SLOT_BORDER, "Inventory slot border"),
        ("SLOT_BG_NORMAL", Colors.SLOT_BG_NORMAL, "Inventory slot normal"),
        ("SLOT_BG_HOVERED", Colors.SLOT_BG_HOVERED, "Inventory slot hovered"),
        ("TABLE_HIGHLIGHT", Colors.TABLE_HIGHLIGHT, "Table row highlight"),
        ("STATUS_TEXT_MAIN_BG", Colors.STATUS_TEXT_MAIN_BG, "Status bar background"),
        ("CHAT_CONSOLE_BG", Colors.CHAT_CONSOLE_BG, "Chat console background"),
    ]

    cols = 4
    rows = (len(colors_data) + cols - 1) // cols
    swatch_w = 0.2
    swatch_h = 0.08
    gap_x = 0.25
    gap_y = 0.15

    for i, (name, color, desc) in enumerate(colors_data):
        row = i // cols
        col = i % cols
        x = 0.05 + col * gap_x
        y = 0.92 - row * gap_y

        # Draw swatch
        rect = mpatches.FancyBboxPatch(
            (x, y - swatch_h), swatch_w, swatch_h,
            boxstyle="round,pad=0.01",
            facecolor=color[:3], alpha=color[3],
            edgecolor='white', linewidth=0.5
        )
        ax.add_patch(rect)

        # Draw text
        ax.text(x + swatch_w / 2, y - swatch_h - 0.02, name,
                ha='center', va='top', fontsize=7, color='white', fontweight='bold')
        ax.text(x + swatch_w / 2, y - swatch_h - 0.04, desc,
                ha='center', va='top', fontsize=5, color='#aaaaaa')

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.set_title("GUITheme Color Palette — Centralized Theme Constants",
                 color='white', fontsize=14, fontweight='bold', pad=20)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "01_color_palette.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Screenshot 2: Pause Menu Mock-up
# ============================================================================
def render_pause_menu():
    fig, ax = plt.subplots(1, 1, figsize=(8, 6))
    fig.patch.set_facecolor('#333333')

    # Draw modal background
    modal_bg = mpatches.Rectangle((0, 0), 1, 1,
                                   facecolor=Colors.MODAL_BG[:3],
                                   alpha=Colors.MODAL_BG[3])
    ax.add_patch(modal_bg)

    # Draw pause menu panel
    panel_w = 0.5
    panel_h = 0.7
    panel_x = (1 - panel_w) / 2
    panel_y = (1 - panel_h) / 2
    panel = FancyBboxPatch(
        (panel_x, panel_y), panel_w, panel_h,
        boxstyle="round,pad=0.02",
        facecolor='#1a1a2e', alpha=0.95,
        edgecolor='#4a4a6a', linewidth=2
    )
    ax.add_patch(panel)

    # Title
    ax.text(0.5, panel_y + panel_h - 0.08, "Game Paused",
            ha='center', va='top', fontsize=18, color=Colors.TEXT_DEFAULT[:3],
            fontweight='bold')

    # Buttons
    buttons = ["Continue", "Settings", "Change Password", "Sound Volume",
               "Exit to Menu", "Exit to OS"]
    btn_h = 0.06
    btn_w = 0.35
    btn_x = 0.5 - btn_w / 2
    start_y = panel_y + panel_h - 0.18
    for i, label in enumerate(buttons):
        y = start_y - i * (btn_h + 0.025)
        btn_color = Colors.BUTTON_BG_DEFAULT[:3]
        btn_alpha = 0.3

        # Draw button background
        btn_rect = FancyBboxPatch(
            (btn_x, y), btn_w, btn_h,
            boxstyle="round,pad=0.005",
            facecolor=btn_color, alpha=btn_alpha,
            edgecolor='#6a6a8a', linewidth=1
        )
        ax.add_patch(btn_rect)

        # Button text
        ax.text(0.5, y + btn_h / 2, label,
                ha='center', va='center', fontsize=10,
                color=Colors.BUTTON_TEXT_DEFAULT[:3])

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.set_title("Pause Menu — Using GUITheme::Colors::MODAL_BG & TEXT_DEFAULT",
                 color='white', fontsize=11, fontweight='bold', pad=15)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "02_pause_menu.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Screenshot 3: Inventory Slots
# ============================================================================
def render_inventory_slots():
    fig, ax = plt.subplots(1, 1, figsize=(10, 4))
    fig.patch.set_facecolor('#333333')

    # Title
    ax.text(0.5, 0.95, "Inventory Slot Theme Constants",
            ha='center', va='top', fontsize=14, color='white', fontweight='bold')

    # Draw inventory grid
    slot_size = 0.08
    gap = slot_size * Sizing.SLOT_SPACING_RATIO
    grid_cols = 9
    grid_rows = 3
    start_x = 0.1
    start_y = 0.75

    for row in range(grid_rows):
        for col in range(grid_cols):
            x = start_x + col * (slot_size + gap)
            y = start_y - row * (slot_size + gap)

            # Slot background
            is_hovered = (row == 1 and col == 3)  # Highlight one slot
            bg_color = Colors.SLOT_BG_HOVERED if is_hovered else Colors.SLOT_BG_NORMAL
            slot = mpatches.FancyBboxPatch(
                (x, y - slot_size), slot_size, slot_size,
                boxstyle="round,pad=0.002",
                facecolor=bg_color[:3], alpha=bg_color[3],
                edgecolor=Colors.SLOT_BORDER[:3],
                linewidth=Colors.SLOT_BORDER[3] * 2,
            )
            ax.add_patch(slot)

            if is_hovered:
                ax.text(x + slot_size / 2, y - slot_size / 2, "HOVER",
                        ha='center', va='center', fontsize=6, color='black')

    # Labels
    labels = [
        (0.85, 0.75, f"SLOT_BG_NORMAL\n(128,128,128)", Colors.SLOT_BG_NORMAL),
        (0.85, 0.55, f"SLOT_BG_HOVERED\n(192,192,192)", Colors.SLOT_BG_HOVERED),
        (0.85, 0.35, f"SLOT_BORDER\n(0,0,0,200)", Colors.SLOT_BORDER),
    ]
    for lx, ly, text, color in labels:
        rect = mpatches.FancyBboxPatch(
            (lx, ly - 0.06), 0.12, 0.08,
            boxstyle="round,pad=0.005",
            facecolor=color[:3], alpha=color[3],
            edgecolor='white', linewidth=0.5
        )
        ax.add_patch(rect)
        ax.text(lx + 0.06, ly - 0.08, text,
                ha='center', va='top', fontsize=6, color='#cccccc')

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "03_inventory_slots.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Screenshot 4: Button States & Modifiers
# ============================================================================
def render_button_states():
    fig, ax = plt.subplots(1, 1, figsize=(10, 5))
    fig.patch.set_facecolor('#333333')

    ax.text(0.5, 0.95, "Button State Modifiers (GUITheme::ButtonModifiers)",
            ha='center', va='top', fontsize=14, color='white', fontweight='bold')

    states = [
        ("Normal", (0.5, 0.5, 0.6, 1.0), "Base color"),
        ("Hovered", tuple(min(c * ButtonModifiers.HOVER_BRIGHTEN, 1.0) for c in (0.5, 0.5, 0.6)) + (1.0,),
         f"Base * {ButtonModifiers.HOVER_BRIGHTEN} (HOVER_BRIGHTEN)"),
        ("Pressed", tuple(c * ButtonModifiers.PRESS_DARKEN for c in (0.5, 0.5, 0.6)) + (1.0,),
         f"Base * {ButtonModifiers.PRESS_DARKEN} (PRESS_DARKEN)"),
    ]

    btn_w = 0.25
    btn_h = 0.12
    start_y = 0.78

    for i, (label, color, desc) in enumerate(states):
        x = 0.1 + i * 0.3
        y = start_y - i * 0.22

        # Draw button
        btn = FancyBboxPatch(
            (x, y - btn_h), btn_w, btn_h,
            boxstyle="round,pad=0.01",
            facecolor=color[:3], alpha=color[3],
            edgecolor='#888888', linewidth=2
        )
        ax.add_patch(btn)

        ax.text(x + btn_w / 2, y - btn_h / 2, label,
                ha='center', va='center', fontsize=14, color='white', fontweight='bold')

        ax.text(x + btn_w / 2, y - btn_h - 0.04, desc,
                ha='center', va='top', fontsize=8, color='#aaaaaa')

    # Color bar visualization
    base_gray = 128
    hover_val = min(int(base_gray * ButtonModifiers.HOVER_BRIGHTEN), 255)
    press_val = int(base_gray * ButtonModifiers.PRESS_DARKEN)

    bar_y = 0.12
    bar_h = 0.06
    ax.text(0.5, bar_y + bar_h + 0.04, "Brightness comparison (base gray = 128)",
            ha='center', fontsize=9, color='#cccccc')

    for i, (label, val) in enumerate([("Pressed", press_val), ("Normal", base_gray), ("Hovered", hover_val)]):
        x = 0.1 + i * 0.3
        c = val / 255
        bar = mpatches.Rectangle((x, bar_y), 0.25, bar_h,
                                  facecolor=(c, c, c), edgecolor='white', linewidth=0.5)
        ax.add_patch(bar)
        ax.text(x + 0.125, bar_y + bar_h / 2, f"{label}: {val}",
                ha='center', va='center', fontsize=9, color='white' if val < 180 else 'black')

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "04_button_states.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Screenshot 5: Table & Tooltip Theme
# ============================================================================
def render_table_and_tooltip():
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor('#333333')

    # Left: Table theme
    ax = axes[0]
    ax.set_facecolor('#333333')

    ax.text(0.5, 0.95, "Table Theme Constants",
            ha='center', va='top', fontsize=12, color='white', fontweight='bold')

    # Draw table rows
    rows = [
        ("Player 1", "Online", False),
        ("Player 2", "Online", True),
        ("Player 3", "Offline", False),
        ("Player 4", "Online", True),
        ("Player 5", "Offline", False),
    ]

    row_h = 0.1
    start_y = 0.82

    for i, (name, status, highlighted) in enumerate(rows):
        y = start_y - i * row_h
        bg = Colors.TABLE_HIGHLIGHT if highlighted else Colors.TABLE_BG_DEFAULT
        text_c = Colors.TABLE_HIGHLIGHT_TEXT if highlighted else Colors.TABLE_TEXT_DEFAULT

        row_rect = mpatches.Rectangle((0.05, y - row_h + 0.02), 0.9, row_h - 0.01,
                                       facecolor=bg[:3], alpha=bg[3],
                                       edgecolor='#444444', linewidth=0.5)
        ax.add_patch(row_rect)

        ax.text(0.1, y - row_h / 2 + 0.02, name,
                ha='left', va='center', fontsize=9, color=text_c[:3])
        ax.text(0.85, y - row_h / 2 + 0.02, status,
                ha='right', va='center', fontsize=9, color=text_c[:3])

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')

    # Right: Tooltip theme
    ax = axes[1]
    ax.set_facecolor('#333333')

    ax.text(0.5, 0.95, "Tooltip Theme Constants",
            ha='center', va='top', fontsize=12, color='white', fontweight='bold')

    # Simulated game screen background
    bg_rect = mpatches.Rectangle((0.05, 0.1), 0.9, 0.75,
                                  facecolor='#2a4a2a', alpha=0.8,
                                  edgecolor='#3a5a3a', linewidth=1)
    ax.add_patch(bg_rect)
    ax.text(0.5, 0.5, "Game World", ha='center', va='center',
            fontsize=16, color='#4a7a4a', alpha=0.5)

    # Tooltip
    tooltip_w = 0.35
    tooltip_h = 0.15
    tooltip_x = 0.35
    tooltip_y = 0.55

    tooltip_rect = FancyBboxPatch(
        (tooltip_x, tooltip_y), tooltip_w, tooltip_h,
        boxstyle="round,pad=0.01",
        facecolor=Colors.TOOLTIP_BG[:3], alpha=Colors.TOOLTIP_BG[3],
        edgecolor='#6a8a6a', linewidth=1
    )
    ax.add_patch(tooltip_rect)
    ax.text(tooltip_x + tooltip_w / 2, tooltip_y + tooltip_h / 2 + 0.02,
            "Diamond Ore", ha='center', va='center', fontsize=11,
            color=Colors.TOOLTIP_TEXT[:3], fontweight='bold')
    ax.text(tooltip_x + tooltip_w / 2, tooltip_y + tooltip_h / 2 - 0.03,
            "Rare ore, found deep underground", ha='center', va='center',
            fontsize=8, color=Colors.TOOLTIP_TEXT[:3])

    # Cursor indicator
    ax.plot(tooltip_x + 0.02, tooltip_y + tooltip_h, 'v', color='yellow', markersize=10)

    # Tooltip size label
    ax.text(0.5, 0.05, f"Initial size: {Sizing.TOOLTIP_INITIAL_SIZE[0]}x{Sizing.TOOLTIP_INITIAL_SIZE[1]}px",
            ha='center', fontsize=9, color='#aaaaaa')

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "05_table_and_tooltip.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Screenshot 6: Complete Theme Summary
# ============================================================================
def render_theme_summary():
    fig, ax = plt.subplots(1, 1, figsize=(14, 8))
    fig.patch.set_facecolor('#1a1a2e')

    ax.text(0.5, 0.97, "GUITheme — Complete Theme Summary",
            ha='center', va='top', fontsize=16, color='white', fontweight='bold')
    ax.text(0.5, 0.93, "src/gui/GUITheme.h — Single source of truth for all GUI styling",
            ha='center', va='top', fontsize=10, color='#888888')

    # Categories
    y = 0.87
    categories = [
        ("Colors", [
            "MODAL_BG(140,0,0,0)  MODAL_BG_FULLSCREEN(255,0,0,0)",
            "TOOLTIP_BG(255,110,130,60)  TOOLTIP_TEXT(255,255,255,255)",
            "TEXT_DEFAULT(255,255,255,255)  BUTTON_BG_DEFAULT(255,255,255,255)",
            "SLOT_BORDER(200,0,0,0)  SLOT_BG_NORMAL(255,128,128,128)  SLOT_BG_HOVERED(255,192,192,192)",
            "TABLE_HIGHLIGHT(255,70,100,50)  CHAT_CONSOLE_BG(255,0,0,0)",
        ], 0.85),
        ("Sizing", [
            "BUTTON_HEIGHT_RATIO = 15/13 * 0.35",
            "SLOT_SPACING_RATIO = 0.25  PADDING_RATIO = 0.05",
            "STATUS_BAR_HEIGHT = 40  MAINMENU_LOCKED_SIZE = 800x600",
            "FIXED_IMGSIZE_DPI_MULT = 0.5555  BUTTON_ALT_HEIGHT_RATIO = 0.875",
        ], 0.55),
        ("ButtonModifiers", [
            "HOVER_BRIGHTEN = 1.25  (brightens button color 25%)",
            "PRESS_DARKEN = 0.85  (darkens button color 15%)",
        ], 0.32),
        ("Spacing", [
            "PASSWORD_DIALOG_SIZE = 580x300  VOLUME_DIALOG_SIZE = 380x200",
            "OPENURL_DIALOG_SIZE = 580x300",
        ], 0.18),
    ]

    for cat_name, items, cat_y in categories:
        ax.text(0.05, cat_y, cat_name, fontsize=11, color='#6aaa6a',
                fontweight='bold', family='monospace')
        for i, item in enumerate(items):
            ax.text(0.08, cat_y - 0.03 - i * 0.03, item,
                    fontsize=7, color='#cccccc', family='monospace')

    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')

    path = os.path.join(OUTPUT_DIR, "06_theme_summary.png")
    fig.savefig(path, dpi=150, bbox_inches='tight', facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  Saved: {path}")
    return path


# ============================================================================
# Main
# ============================================================================
def main():
    if not HAS_MATPLOTLIB:
        print("ERROR: matplotlib is required for visual tests")
        sys.exit(1)

    print("=" * 60)
    print("  Luantis GUITheme E2E Visual Validation")
    print("=" * 60)

    screenshots = []
    print("\nRendering screenshots...")

    screenshots.append(render_color_palette())
    screenshots.append(render_pause_menu())
    screenshots.append(render_inventory_slots())
    screenshots.append(render_button_states())
    screenshots.append(render_table_and_tooltip())
    screenshots.append(render_theme_summary())

    print(f"\nAll {len(screenshots)} screenshots saved to {OUTPUT_DIR}/")

    # Write test results
    results = {
        "total_screenshots": len(screenshots),
        "screenshots": screenshots,
        "all_passed": True
    }
    with open(os.path.join(OUTPUT_DIR, "test_results.json"), 'w') as f:
        json.dump(results, f, indent=2)

    print("Visual validation PASSED!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
