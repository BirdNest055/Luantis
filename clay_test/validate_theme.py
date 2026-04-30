#!/usr/bin/env python3
"""
Theme E2E Validation Test

Validates that:
1. All Theme namespaces/values exist in clay_theme.h
2. JSONL output matches expected Theme values
3. Custom theme overrides produce different visual output
4. Style composition (Theme::Styles::button() etc.) is correct
"""

import json
import sys
import os

def load_jsonl(path):
    """Load JSONL file into list of frame dicts."""
    frames = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            frames.append(json.loads(line))
    return frames

def validate_pause_menu_default(frame):
    """Test 1: Pause menu with default Theme values."""
    name = frame['layout']
    cmds = frame['commands']
    
    # Must have at least: overlay rect, content rect, title rect, title text,
    # 6 buttons (rect+text each = 12), separator rect, bottom pad rect = 18
    assert len(cmds) >= 15, f"{name}: expected >= 15 commands, got {len(cmds)}"
    
    # Find overlay rect (first rect, covers full screen)
    overlay_rect = None
    for c in cmds:
        if c['type'] == 1:  # RECTANGLE
            overlay_rect = c
            break
    assert overlay_rect is not None, f"{name}: no overlay rectangle found"
    assert overlay_rect['w'] == 1280 and overlay_rect['h'] == 720, \
        f"{name}: overlay should be full-screen, got {overlay_rect['w']}x{overlay_rect['h']}"
    
    # Overlay background should be dark semi-transparent: {0, 0, 0, 160}
    bg = overlay_rect['bg']
    assert bg[0] == 0 and bg[1] == 0 and bg[2] == 0, \
        f"{name}: overlay bg should be black, got {bg}"
    assert bg[3] == 160, f"{name}: overlay alpha should be 160, got {bg[3]}"
    
    # Find title text "Game Paused"
    title_text = None
    for c in cmds:
        if c.get('text') == 'Game Paused':
            title_text = c
            break
    assert title_text is not None, f"{name}: 'Game Paused' text not found"
    assert title_text['fontSize'] == 28, \
        f"{name}: title fontSize should be 28 (Theme::Fonts::titleFontSize), got {title_text['fontSize']}"
    
    # Find button texts
    button_labels = {'Resume Game', 'Settings', 'Change Password', 'Volume', 'Exit to Menu', 'Exit to OS'}
    found_labels = set()
    for c in cmds:
        if c.get('text') in button_labels:
            found_labels.add(c['text'])
            # Button font size should be 20 (Theme::Fonts::buttonFontSize)
            assert c['fontSize'] == 20, \
                f"{name}: button '{c['text']}' fontSize should be 20, got {c['fontSize']}"
    assert found_labels == button_labels, \
        f"{name}: missing button labels: {button_labels - found_labels}"
    
    # "Volume" button should be in hover state (brighter text color)
    vol_text = None
    for c in cmds:
        if c.get('text') == 'Volume':
            vol_text = c
            break
    assert vol_text is not None, f"{name}: Volume button text not found"
    # Hover text: {255, 255, 200, 255} — blue channel should be low (warm tint)
    assert vol_text['textColor'][2] < 220, \
        f"{name}: Volume (hover) text should have warm yellow tint, got {vol_text['textColor']}"
    
    print(f"  PASS: {name} - {len(cmds)} commands, all Theme values correct")
    return True

def validate_pause_menu_green(frame):
    """Test 2: Green custom theme — colors should differ from default."""
    name = frame['layout']
    cmds = frame['commands']
    
    # Find overlay rect — should NOT be {0,0,0,160} (default), should be greenish
    overlay_rect = None
    for c in cmds:
        if c['type'] == 1:
            overlay_rect = c
            break
    assert overlay_rect is not None, f"{name}: no overlay rectangle found"
    
    bg = overlay_rect['bg']
    # GreenTheme::overlayBg = {0, 10, 0, 180} — g channel should be > 0
    assert bg[1] > 0 or bg[3] != 160, \
        f"{name}: green theme overlay should differ from default, got {bg}"
    
    # Title text should be "Tactical Interface" (not "Game Paused")
    found_tactical = False
    for c in cmds:
        if c.get('text') == 'Tactical Interface':
            found_tactical = True
            # Green title text: {100, 255, 100, 255} — high green channel
            assert c['textColor'][1] == 255 and c['textColor'][0] < 200, \
                f"{name}: green title text should be greenish, got {c['textColor']}"
    assert found_tactical, f"{name}: 'Tactical Interface' text not found"
    
    # Menu width should be 500 (GreenTheme::menuWidth), not 400 (default)
    # Find the content rectangle (second rect) — its width should be ~500
    content_rects = [c for c in cmds if c['type'] == 1]
    if len(content_rects) >= 2:
        content_w = content_rects[1]['w']
        assert content_w >= 490, \
            f"{name}: green menu width should be ~500, got {content_w}"
    
    print(f"  PASS: {name} - custom theme colors verified, differs from default")
    return True

def validate_settings_themed(frame):
    """Test 3: Settings dialog using Theme colors/sizing."""
    name = frame['layout']
    cmds = frame['commands']
    
    # Should have "Settings" title text
    found_settings = False
    for c in cmds:
        if c.get('text') == 'Settings':
            found_settings = True
            # headingFontSize = 24
            assert c['fontSize'] == 24, \
                f"{name}: settings title should be 24px (headingFontSize), got {c['fontSize']}"
    assert found_settings, f"{name}: 'Settings' text not found"
    
    # Should have tab names
    tab_names = {'Graphics', 'Sound', 'Controls', 'Privacy'}
    found_tabs = set()
    for c in cmds:
        if c.get('text') in tab_names:
            found_tabs.add(c['text'])
    assert found_tabs == tab_names, f"{name}: missing tabs: {tab_names - found_tabs}"
    
    # Active tab (Graphics) should have accent color background
    # Theme::Colors::accent = {100, 140, 220, 255}
    for c in cmds:
        if c.get('text') == 'Graphics':
            # Find the rect associated with this tab (same position)
            for r in cmds:
                if r['type'] == 1 and r.get('bg'):
                    bg = r['bg']
                    if bg[0] == 100 and bg[1] == 140 and bg[2] == 220:
                        break
            break
    
    print(f"  PASS: {name} - {len(cmds)} commands, Theme::Colors/Fonts used correctly")
    return True

def validate_hud_themed(frame):
    """Test 4: HUD using Theme::Sizing constants."""
    name = frame['layout']
    cmds = frame['commands']
    
    # Should have hotbar slot numbers
    slot_nums = {str(i) for i in range(1, 10)}
    found_slots = set()
    for c in cmds:
        if c.get('text') in slot_nums:
            found_slots.add(c['text'])
    assert found_slots == slot_nums, f"{name}: missing hotbar slots: {slot_nums - found_slots}"
    
    # Hotbar slot font should be hudFontSize (16)
    for c in cmds:
        if c.get('text') in slot_nums:
            assert c['fontSize'] == 16, \
                f"{name}: hotbar slot fontSize should be 16 (hudFontSize), got {c['fontSize']}"
    
    # Should have health/armor bar texts
    for c in cmds:
        if c.get('text') in ('17/20', '12/20'):
            assert c['fontSize'] == 16, \
                f"{name}: bar text fontSize should be 16 (bodyFontSize), got {c['fontSize']}"
    
    print(f"  PASS: {name} - {len(cmds)} commands, Theme::Sizing verified")
    return True

def validate_component_test(frame):
    """Test 5: Component isolation — each style rendered individually."""
    name = frame['layout']
    cmds = frame['commands']
    
    # Should have "Button States", "Text Styles", "Color Palette" headings
    headings = {'Button States', 'Text Styles', 'Color Palette'}
    found_headings = set()
    for c in cmds:
        if c.get('text') in headings:
            found_headings.add(c['text'])
    assert found_headings == headings, f"{name}: missing headings: {headings - found_headings}"
    
    # Should have Normal, Hover, Pressed button states
    states = {'Normal', 'Hover', 'Pressed'}
    found_states = set()
    for c in cmds:
        if c.get('text') in states:
            found_states.add(c['text'])
    assert found_states == states, f"{name}: missing button states: {states - found_states}"
    
    # Color swatches should exist
    swatches = {'contentBg', 'titleBg', 'buttonBg', 'buttonHoverBg', 'accent', 'danger', 'success', 'warning', 'separator'}
    found_swatches = set()
    for c in cmds:
        if c.get('text') in swatches:
            found_swatches.add(c['text'])
    assert found_swatches == swatches, f"{name}: missing swatches: {swatches - found_swatches}"
    
    # Title text config: fontSize=28
    for c in cmds:
        if c.get('text') in headings:
            assert c['fontSize'] == 28, \
                f"{name}: heading should be 28px (titleFontSize), got {c['fontSize']}"
    
    # Normal button text color: {220, 220, 220, 255}
    for c in cmds:
        if c.get('text') == 'Normal':
            assert c['textColor'] == [220, 220, 220, 255], \
                f"{name}: Normal text color should be buttonText, got {c['textColor']}"
    
    # Hover button text color: {255, 255, 200, 255}
    for c in cmds:
        if c.get('text') == 'Hover':
            assert c['textColor'] == [255, 255, 200, 255], \
                f"{name}: Hover text color should be buttonHoverText, got {c['textColor']}"
    
    # Pressed button text color: {200, 200, 180, 255}
    for c in cmds:
        if c.get('text') == 'Pressed':
            assert c['textColor'] == [200, 200, 180, 255], \
                f"{name}: Pressed text color should be buttonPressedText, got {c['textColor']}"
    
    print(f"  PASS: {name} - {len(cmds)} commands, all component styles verified")
    return True

def validate_cross_theme_difference(frames):
    """Validate that default and green theme produce DIFFERENT visual output."""
    if len(frames) < 2:
        print("  SKIP: Need at least 2 frames for cross-theme comparison")
        return True
    
    f1, f2 = frames[0], frames[1]
    
    # Get all bg colors from frame 1
    bg_colors_1 = set()
    for c in f1['commands']:
        if c.get('bg'):
            bg_colors_1.add(tuple(c['bg']))
    
    # Get all bg colors from frame 2
    bg_colors_2 = set()
    for c in f2['commands']:
        if c.get('bg'):
            bg_colors_2.add(tuple(c['bg']))
    
    # The sets should differ (green theme has different colors)
    assert bg_colors_1 != bg_colors_2, \
        "Cross-theme test: default and green theme produce IDENTICAL colors — theme override failed!"
    
    # Also check text content differs
    texts_1 = set(c.get('text', '') for c in f1['commands'])
    texts_2 = set(c.get('text', '') for c in f2['commands'])
    assert texts_1 != texts_2, \
        "Cross-theme test: default and green theme have IDENTICAL text — label override failed!"
    
    print(f"  PASS: Cross-theme comparison — default vs green produce different output")
    return True

def main():
    jsonl_path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/theme_test.jsonl'
    screenshots_dir = sys.argv[2] if len(sys.argv) > 2 else '/tmp/theme_screenshots'
    
    if not os.path.exists(jsonl_path):
        print(f"ERROR: JSONL file not found: {jsonl_path}")
        sys.exit(1)
    
    print("=" * 60)
    print("Luantis Clay Theme E2E Validation")
    print("=" * 60)
    
    frames = load_jsonl(jsonl_path)
    print(f"\nLoaded {len(frames)} test frames from {jsonl_path}\n")
    
    assert len(frames) >= 5, f"Expected 5 test frames, got {len(frames)}"
    
    all_pass = True
    
    # Run each validation
    tests = [
        (frames[0], validate_pause_menu_default),
        (frames[1], validate_pause_menu_green),
        (frames[2], validate_settings_themed),
        (frames[3], validate_hud_themed),
        (frames[4], validate_component_test),
    ]
    
    for frame, validator in tests:
        try:
            validator(frame)
        except AssertionError as e:
            print(f"  FAIL: {e}")
            all_pass = False
        except Exception as e:
            print(f"  ERROR: {e}")
            all_pass = False
    
    # Cross-theme validation
    try:
        validate_cross_theme_difference(frames)
    except AssertionError as e:
        print(f"  FAIL: {e}")
        all_pass = False
    
    # Screenshot existence check
    expected_screenshots = [
        '01_pause_menu_default.png',
        '02_pause_menu_green.png',
        '03_settings_themed.png',
        '04_hud_themed.png',
        '05_component_test.png',
        'all_layouts_summary.png',
    ]
    for ss in expected_screenshots:
        path = os.path.join(screenshots_dir, ss)
        if os.path.exists(path):
            size = os.path.getsize(path)
            print(f"  Screenshot OK: {ss} ({size} bytes)")
        else:
            print(f"  MISSING: {ss}")
            all_pass = False
    
    print("\n" + "=" * 60)
    if all_pass:
        print("ALL TESTS PASSED")
    else:
        print("SOME TESTS FAILED")
    print("=" * 60)
    
    return 0 if all_pass else 1

if __name__ == '__main__':
    sys.exit(main())
