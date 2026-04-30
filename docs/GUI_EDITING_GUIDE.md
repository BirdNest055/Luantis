# Luantis GUI Editing Guide

This document describes exactly which files to edit to change the GUI of all
windows, buttons, tabs, dialogs, colors, fonts, and textures in the Luantis
client. It covers the main menu, in-game HUD, pause menu, and the C++ widget
layer that renders everything.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Main Menu Tabs](#2-main-menu-tabs)
3. [Main Menu Dialogs](#3-main-menu-dialogs)
4. [Main Menu Theming & Colors](#4-main-menu-theming--colors)
5. [Keypair Manager GUI](#5-keypair-manager-gui)
6. [In-Game GUI](#6-in-game-gui)
7. [Settings Dialog](#7-settings-dialog)
8. [Pause Menu](#8-pause-menu)
9. [C++ GUI Widgets (src/gui/)](#9-c-gui-widgets-srcgui)
10. [Textures & Images](#10-textures--images)
11. [Formspec Reference](#11-formspec-reference)
12. [How to Add a New Dialog](#12-how-to-add-a-new-dialog)
13. [Common Patterns & Pitfalls](#13-common-patterns--pitfalls)

---

## 1. Architecture Overview

The Luantis GUI uses Luanti's **formspec** system — a declarative markup
language that describes UI elements as strings. Lua code generates formspec
strings, and the C++ engine (`src/gui/guiFormSpecMenu.cpp`) parses and renders
them using Irrlicht.

```
┌─────────────────────────────────────────────────────────────────┐
│ Lua Layer (builtin/)                                            │
│                                                                 │
│  builtin/mainmenu/init.lua  ──►  Creates TabView, loads tabs   │
│       │                                                         │
│       ├── tab_local.lua     (Start Game tab)                    │
│       ├── tab_online.lua    (Join Game tab)                     │
│       ├── tab_content.lua   (Content tab)                       │
│       ├── tab_about.lua     (About tab)                         │
│       │                                                         │
│       ├── dlg_keypair_manager.lua  (Keypair dialog)             │
│       ├── dlg_register.lua         (Registration dialog)        │
│       ├── dlg_create_world.lua     (World creation dialog)      │
│       └── ... (17 dialogs total)                                │
│                                                                 │
│  builtin/common/menu.lua    ──►  Color constants (mt_color_*)   │
│  builtin/mainmenu/game_theme.lua ──► Background/sky theming    │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ C++ Layer (src/gui/)                                            │
│                                                                 │
│  guiFormSpecMenu.cpp  ──► Parses formspec strings, renders     │
│  guiEngine.cpp        ──► Main menu render loop bridge          │
│  guiTable.cpp         ──► Table widget (columns, rows)          │
│  guiButton.cpp        ──► Button widget with styles             │
│  StyleSpec.h          ──► CSS-like style property system        │
│  ... (30+ widget files)                                         │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│ Assets                                                          │
│                                                                 │
│  textures/base/pack/  ──► Menu textures (icons, backgrounds)   │
│  fonts/               ──► Arimo, Cousine, DroidSansFallback    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Key Layout Constants

Defined in `builtin/mainmenu/init.lua`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `MAIN_TAB_W` | 15.5 | Main menu form width |
| `MAIN_TAB_H` | 7.1 | Main menu form height |
| `TABHEADER_H` | computed | Tab header height |
| `GAMEBAR_H` | computed | Game selection bar height |

### Tab Framework

Defined in `builtin/fstk/tabview.lua`:
- All main menu tabs use `formspec_version[6]` by default
- Tabs are added in order in `init.lua` (Start Game → Join Game → Content → About)
- The Settings gear icon is an "end button" at the top-right

---

## 2. Main Menu Tabs

### Start Game Tab — `builtin/mainmenu/tab_local.lua`

| What to change | Where in the file |
|---|---|
| Tab layout / form structure | `tab_local.get_formspec()` function |
| World list table columns | `tablecolumns[text,...]` and `render_worldlist()` |
| Game bar (game icons) | Game bar rendering section |
| Play/Host buttons | Button definitions near bottom |
| Creative/Damage/Host checkboxes | Checkbox definitions |
| New/Delete/Configure World buttons | Button row below world list |

### Join Game Tab — `builtin/mainmenu/tab_online.lua`

| What to change | Where in the file |
|---|---|
| Tab layout / form structure | `tab_online.get_formspec()` function |
| Server list table columns | `tablecolumns[image;color;text;...]` at line ~281 |
| Server list row rendering | `render_serverlist_row()` in `common.lua` |
| Login area (Name, Password, Keys button) | Lines 146-199 |
| Keypair auth layout | Conditional block when `keypair_auth` is enabled |
| Connect/Login/Register buttons | Lines 190-199 |
| Search/filter fields | Search input section |
| Favorite/URL/Mods/Clients buttons | Button definitions per-server |
| Ping/players/flags icons | `tablecolumns` image references |

### Content Tab — `builtin/mainmenu/tab_content.lua`

| What to change | Where in the file |
|---|---|
| Package list layout | `get_formspec()` function |
| Installed packages rendering | Package list section |
| Update indicators | `cdb_update_cropped.png` references |
| Uninstall/Use buttons | Button definitions |

### About Tab — `builtin/mainmenu/tab_about.lua`

| What to change | Where in the file |
|---|---|
| Logo image | `logo.png` reference |
| Version text | Version string display |
| Credits content | Hypertext rendering section |
| Homepage URL button | URL button definition |

---

## 3. Main Menu Dialogs

All dialogs follow the pattern: a `formspec()` function that returns a
formspec string, and a `buttonhandler()` function that processes button
clicks. They are created using `dialog_create(name, formspec_func,
buttonhandler_func, eventhandler_func)`.

| Dialog | File | Formspec Ver | Purpose |
|--------|------|-------------|---------|
| Keypair Manager | `builtin/mainmenu/dlg_keypair_manager.lua` | v4 | Ed25519 keypair management |
| Server Registration | `builtin/mainmenu/dlg_register.lua` | v4 | Register account on server |
| Create World | `builtin/mainmenu/dlg_create_world.lua` | v6* | New world creation |
| Configure World | `builtin/mainmenu/dlg_config_world.lua` | v6* | World mod configuration |
| Delete World | `builtin/mainmenu/dlg_delete_world.lua` | legacy | Delete world confirmation |
| Confirm Exit | `builtin/mainmenu/dlg_confirm_exit.lua` | v10 | Exit confirmation |
| Version Info | `builtin/mainmenu/dlg_version_info.lua` | v3 | New version notification |
| Clients List | `builtin/mainmenu/dlg_clients_list.lua` | v8 | Connected players list |
| Server Mods | `builtin/mainmenu/dlg_server_list_mods.lua` | v8 | Server mod list |
| Keybinding Migration | `builtin/mainmenu/dlg_rebind_keys.lua` | v6 | Keybinding upgrade prompt |
| Reinstall MTG | `builtin/mainmenu/dlg_reinstall_mtg.lua` | v6 | Minetest Game reinstall |
| Rename Modpack | `builtin/mainmenu/dlg_rename_modpack.lua` | legacy | Modpack rename |
| Delete Content | `builtin/mainmenu/dlg_delete_content.lua` | legacy | Content deletion confirm |
| ContentDB Browser | `builtin/mainmenu/content/dlg_contentdb.lua` | v7 | Online content browser |
| Package Details | `builtin/mainmenu/content/dlg_package.lua` | v7 | Package info/reviews |
| Install Package | `builtin/mainmenu/content/dlg_install.lua` | v3 | Package install dialog |
| Overwrite Confirm | `builtin/mainmenu/content/dlg_overwrite.lua` | legacy | Overwrite confirmation |

(*inherits the tab's default formspec_version)

### How Dialogs Are Loaded

Dialogs are loaded in `builtin/mainmenu/init.lua` via `dofile()`. To add a
new dialog:
1. Create a new `dlg_yourfeature.lua` file
2. Add `dofile(menupath .. DIR_DELIM .. "dlg_yourfeature.lua")` in `init.lua`
3. Open it from a tab or another dialog using `dialog_create(...)`

---

## 4. Main Menu Theming & Colors

### Color Constants — `builtin/common/menu.lua`

| Variable | Color | Usage |
|----------|-------|-------|
| `mt_color_grey` | `#AAAAAA` | Incompatible/disabled text |
| `mt_color_blue` | `#6389FF` | Links, highlights |
| `mt_color_lightblue` | `#99CCFF` | Secondary highlights |
| `mt_color_green` | `#72FF63` | Success indicators |
| `mt_color_dark_green` | `#25C191` | Active/online indicators |
| `mt_color_orange` | `#FF8800` | Warnings |
| `mt_color_red` | `#FF3300` | Errors, delete actions |

### Background & Sky — `builtin/mainmenu/game_theme.lua`

| What to change | Where |
|---|---|
| Dark theme sky/cloud colors | `COLORS.dark.sky`, `COLORS.dark.clouds` |
| Light theme sky/cloud colors | `COLORS.light.sky`, `COLORS.light.clouds` |
| Background images | `set_engine()`, `set_game()` functions |
| Menu music | `set_game()` — plays `game/menu/menu_music.ogg` |
| Random backgrounds | Files named `background.1.png`, `background.2.png` in game's `menu/` dir |
| Theme selection | `menu_theme` setting (dark/light) |

### Formspec Styles (CSS-like)

Formspec styles are defined inline in Lua using the `style[]` and
`style_type[]` elements. Properties are defined in C++ `src/gui/StyleSpec.h`:

| Property | Values | Example |
|----------|--------|---------|
| `bgcolor` | Color name or hex | `style[btn;bgcolor=red]` |
| `bgcolor_hovered` | Color | `style[btn;bgcolor_hovered=#6663]` |
| `bgcolor_pressed` | Color | `style[btn;bgcolor_pressed=#3333]` |
| `textcolor` | Color | `style[label;textcolor=#0E0]` |
| `bgimg` | Texture path | `style[btn;bgimg=button.png]` |
| `bgimg_hovered` | Texture path | `style[btn;bgimg_hovered=hover.png]` |
| `bgimg_pressed` | Texture path | `style[btn;bgimg_pressed=press.png]` |
| `font` | normal/bold/mono/italic | `style_type[label;font=bold]` |
| `font_size` | Number, +N, *N | `style_type[label;font_size=+24]` |
| `padding` | Number | `style[btn;padding=6]` |
| `border` | bool | `style[btn;border=false]` |
| `alpha` | 0-255 | `style[img;alpha=128]` |
| `noclip` | bool | `style[end_button;noclip=true]` |
| `sound` | Sound name | `style[btn;sound=click]` |
| `spacing` | Number | Container spacing |

**Style states**: `default`, `focused`, `hovered`, `pressed`
- Syntax: `style[name:hovered;bgcolor=#666]` or `style_type[button:pressed;...]`

---

## 5. Keypair Manager GUI

**File**: `builtin/mainmenu/dlg_keypair_manager.lua`

This is the Ed25519 keypair management dialog introduced in v9.37.

### Layout Sections (top to bottom)

1. **Title** — "Keypair Manager" label
2. **Disabled warning** — Orange box if keypair_auth is off
3. **Current user status** — Dark box showing username + keypair status
4. **No keypair warning** — Orange box if current user has no key
5. **Your Public Key** — Dark box with truncated base64 public key
6. **All Keypairs** — Table with columns: Username, Public Key, First Seen, Last Used
7. **Registered Servers** — Table with columns: Server, Username, Created, Last Used
8. **Regenerate section** — Button + confirmation warning/success messages
9. **Close button**

### What to Change

| What | Where | Notes |
|------|-------|-------|
| Dialog title | `fgettext("Keypair Manager")` | |
| Public key label | `fgettext("Your Public Key")` | |
| Keypair table columns | `tablecolumns[...]` | Comma within column, semicolon between columns |
| Keypair table data | `escape_row()` helper | **CRITICAL**: Escape each cell individually, never the whole row |
| Server table columns | `tablecolumns[...]` | Same comma/semicolon rule |
| Table heights | `keypair_table_h`, `server_table_h` | Dynamic based on item count |
| Regenerate warning | `fgettext("WARNING: ...")` | Two-line warning text |
| Button labels | `fgettext("Delete Selected Keypair")` etc. | |
| Section spacing | Y-position variables | Layout is computed top-to-bottom |
| No-key message | `fgettext("No keypair for '%s'...")` | |

### Lua API Functions Used

These are exposed as `core.*` functions from C++ (`src/script/lua_api/l_mainmenu.cpp`):

| Function | Returns | Purpose |
|----------|---------|---------|
| `core.keypair_list_keypairs()` | Array of `{username, public_key_base64}` | List all keypairs |
| `core.keypair_get_server_list()` | Array of `{server, username, created_at, last_used_at}` | List server registrations |
| `core.keypair_delete_keypair(username)` | bool | Delete keypair for user |
| `core.keypair_forget_server(addr)` | bool | Remove server registration |
| `core.keypair_regenerate(username)` | bool | Regenerate keypair |
| `core.keypair_get_public_key_base64(username)` | string | Get public key |
| `core.keypair_has_keypair(username)` | bool | Check if keypair exists |
| `core.keypair_is_enabled()` | bool | Check if keypair_auth setting is on |

### How to Open the Dialog

From `tab_online.lua`, the "Keys" button:
```lua
if fields.btn_keypair_manager then
    local dlg = create_keypair_manager_dialog()
    dlg:set_parent(this)
    this:hide()
    dlg:show()
    return true
end
```

---

## 6. In-Game GUI

### Death Screen — `builtin/game/death_screen.lua`

| What | Where |
|------|-------|
| Death message text | `fgettext("You died")` |
| Background tint | `bgcolor[#320000b4]` |
| Respawn button | Button definition |

### HUD Elements — `builtin/game/hud.lua`

| What | Where |
|------|-------|
| Health bar | `hud_replace_builtin("health", ...)` — uses `heart.png` |
| Breath bar | `hud_replace_builtin("breath", ...)` — uses `bubble.png` |
| Minimap | `hud_replace_builtin("minimap", ...)` |
| Hotbar | `hud_replace_builtin("hotbar", ...)` |

### Chat/Info Forms — `builtin/common/information_formspecs.lua`

| What | Where |
|------|-------|
| Help command output | Chat command tree rendering |
| Privs command output | Privilege list rendering |

---

## 7. Settings Dialog

**File**: `builtin/common/settings/dlg_settings.lua`

| What | Where |
|------|-------|
| Settings page list | Left sidebar with page navigation |
| Setting component rendering | Dynamic formspec generation |
| Search functionality | Search field at top |
| Technical names toggle | "Show technical names" checkbox |
| Advanced settings toggle | "Show advanced settings" checkbox |
| Page categories | Accessibility, controls, graphics, audio, etc. |

**Formspec version**: 6
**Style overrides**: `style_type[button;border=false;bgcolor=#3333]`

---

## 8. Pause Menu

**Directory**: `builtin/pause_menu/`

| File | Purpose |
|------|---------|
| `init.lua` | Pause menu initialization, sets `defaulttexturedir = ""` |
| `register.lua` | Registers `core.registered_on_formspec_input` event handler |

The pause menu settings dialog uses `builtin/common/settings/dlg_settings.lua`.

---

## 9. C++ GUI Widgets (src/gui/)

The C++ layer in `src/gui/` implements the actual rendering. You only need to
edit these files if you want to change how a formspec element is rendered
(e.g., change the default button appearance, add a new element type).

### Core Files

| File | Purpose |
|------|---------|
| `guiFormSpecMenu.cpp/.h` | **Formspec parser** — parses formspec strings, creates widgets, handles events, scrolling |
| `guiEngine.cpp/.h` | **Main menu engine** — bridges C++ engine to Lua mainmenu |
| `StyleSpec.h` | **Style system** — all CSS-like properties, state propagation |

### Widget Files

| File | Formspec Element | Notes |
|------|-----------------|-------|
| `guiButton.cpp/.h` | `button`, `button_exit` | Base button with style support |
| `guiButtonImage.cpp/.h` | `image_button` | Image background button |
| `guiButtonItemImage.cpp/.h` | `item_image_button` | Inventory item button |
| `guiButtonKey.cpp/.h` | Key binding button | Used in settings |
| `guiAnimatedImage.cpp/.h` | `animated_image` | Animated sprite |
| `guiItemImage.cpp/.h` | Item stack renderer | Inventory display |
| `guiBackgroundImage.cpp/.h` | `background` | Background/overlay image |
| `guiBox.cpp/.h` | `box` | Colored rectangle |
| `guiEditBoxWithScrollbar.cpp/.h` | `field`, `textarea` | Text input with scroll |
| `guiHyperText.cpp/.h` | `hypertext` | Rich text with links |
| `guiInventoryList.cpp/.h` | `list` | Inventory grid |
| `guiScrollBar.cpp/.h` | `scrollbar` | Scroll bar widget |
| `guiScrollContainer.cpp/.h` | `scroll_container` | Scrollable container |
| `guiTable.cpp/.h` | `table` | Data table with columns |
| `guiScene.cpp/.h` | `model` | 3D scene preview |
| `guiChatConsole.cpp/.h` | In-game chat | Console overlay |
| `guiOpenURL.cpp/.h` | URL dialog | URL opening confirmation |
| `guiPasswordChange.cpp/.h` | Password change | Password change dialog |
| `guiPathSelectMenu.cpp/.h` | File picker | Path selection dialog |
| `guiVolumeChange.cpp/.h` | Volume control | Volume slider dialog |
| `touchcontrols.cpp/.h` | Touch overlay | Touch screen buttons |
| `touchscreenlayout.cpp/.h` | Touch layout | Touch button layout engine |
| `touchscreeneditor.cpp/.h` | Touch editor | Touch layout editor dialog |

### Font Engine — `src/client/fontengine.cpp`

| Font | File | Usage |
|------|------|-------|
| Arimo | `fonts/Arimo*.ttf` | Default UI font (Regular/Bold/Italic/BoldItalic) |
| Cousine | `fonts/Cousine*.ttf` | Monospace font (Regular/Bold/Italic/BoldItalic) |
| DroidSansFallback | `fonts/DroidSansFallbackFull.ttf` | CJK fallback font |

---

## 10. Textures & Images

### Default Texture Directory

`textures/base/pack/` — referenced as `defaulttexturedir` in Lua code.

### Common Textures

| Texture | Used In | Purpose |
|---------|---------|---------|
| `logo.png` | tab_about | Engine logo |
| `settings_btn.png` | init.lua | Settings gear icon |
| `plus.png` | tab_local | "Install games" button |
| `search.png` | Various | Search icon |
| `clear.png` | Various | Clear search icon |
| `refresh.png` | tab_online | Refresh server list |
| `blank.png` | tab_online, etc. | Empty/placeholder |
| `server_favorite.png` | tab_online | Favorite server icon |
| `server_ping_1-4.png` | tab_online | Ping indicators |
| `checkbox_16/64.png` | Various | Checkbox icons |
| `menu_header.png` | game_theme | Menu header bar |
| `menu_overlay.png` | game_theme | Menu overlay image |
| `menu_background.png` | game_theme | Menu background |
| `progress_bar.png` | Various | Progress indicator |

### Game-Specific Textures

Games can provide their own menu textures in their `menu/` directory:
- `menu/background.png` — Main menu background
- `menu/background.1.png`, `background.2.png` — Random backgrounds
- `menu/header.png` — Header image
- `menu/footer.png` — Footer image
- `menu/overlay.png` — Overlay image
- `menu/menu_music.ogg` — Background music

---

## 11. Formspec Reference

### Common Elements

```
-- Container (groups elements, relative positioning)
container[X,Y] ... container_end[]

-- Labels
label[X,Y;TEXT]
vertlabel[X,Y;TEXT]

-- Buttons
button[X,Y;W,H;NAME;LABEL]
button_exit[X,Y;W,H;NAME;LABEL]       -- Closes dialog on click
image_button[X,Y;W,H;TEXTURE;NAME;LABEL]
item_image_button[X,Y;W,H;ITEM;NAME;LABEL]

-- Text Input
field[X,Y;W,H;NAME;LABEL;DEFAULT]
pwdfield[X,Y;W,H;NAME;LABEL]
textarea[X,Y;W,H;NAME;LABEL;DEFAULT]

-- Selection
dropdown[X,Y;W,H;NAME;OPTION1,OPTION2,...;SELECTED_IDX]
tabheader[X,Y;NAME;TAB1,TAB2,...;SELECTED_IDX;TRANSPARENT;BORDER]

-- Display
box[X,Y;W,H;COLOR]                     -- Colored rectangle
image[X,Y;W,H;TEXTURE]                 -- Image display
animated_image[X,Y;W,H;NAME;TEXTURE;FRAME_COUNT;FRAME_DURATION]
item_image[X,Y;W,H;ITEM]
hypertext[X,Y;W,H;NAME;TEXT]           -- Rich text

-- Data Table
tablecolumns[TYPE,OPT1,OPT2;TYPE,OPT1;...]  -- Column definitions
table[X,Y;W,H;NAME;HEADER1,HEADER2,...,DATA1,DATA2,...;SELECTED]

-- Lists
list[LOCATION;LISTNAME;X,Y;W,H;START_IDX]

-- Scroll
scrollbar[X,Y;W,H;ORIENTATION;NAME;VALUE]
scroll_container[X,Y;W,H;NAME;ORIENTATION;MAX] ... scroll_container_end[]

-- Styling
style[NAME;PROP1=VAL1,PROP2=VAL2]              -- Style by element name
style_type[TYPE;PROP1=VAL1,PROP2=VAL2]          -- Style by element type
style[NAME:STATE;PROP1=VAL1]                     -- Style with state (hovered, pressed, focused)

-- Layout
bgcolor[COLOR;FULLSCREEN]
padding[X,Y]
set_focus[NAME;FORCE]
```

### Table Column Types

| Type | Description |
|------|-------------|
| `text` | Text column |
| `image` | Image column (use `0=texture.png,1=...` for icon mapping) |
| `color` | Text color for the next `span` columns |
| `indent` | Indent level |

### Column Options (comma-separated within column)

| Option | Values | Description |
|--------|--------|-------------|
| `align=VALUE` | left, center, right, inline | Text alignment |
| `width=N` | Number | Column width (proportional) |
| `padding=N` | Number | Cell padding |
| `span=N` | Number | How many columns this affects (for color) |
| `tooltip=TEXT` | String | Column tooltip |

---

## 12. How to Add a New Dialog

### Step-by-step

1. **Create the dialog file**: `builtin/mainmenu/dlg_yourfeature.lua`

```lua
local function yourfeature_formspec(dialogdata)
    return table.concat({
        "formspec_version[4]",
        "size[10,8]",
        "label[0.375,0.5;Your Feature]",
        -- ... your elements ...
        "button[7,7;2.5,0.8;btn_close;", fgettext("Close"), "]",
    }, "")
end

local function yourfeature_buttonhandler(this, fields)
    if fields.btn_close or fields.quit then
        this:delete()
        return true
    end
    -- Handle your buttons here
    return false
end

function create_yourfeature_dialog()
    return dialog_create("dlg_yourfeature",
        yourfeature_formspec,
        yourfeature_buttonhandler,
        nil)
end
```

2. **Register it in init.lua**: Add at the top with other `dofile` calls:
```lua
dofile(menupath .. DIR_DELIM .. "dlg_yourfeature.lua")
```

3. **Open it from a tab or another dialog**:
```lua
if fields.btn_yourfeature then
    local dlg = create_yourfeature_dialog()
    dlg:set_parent(this)
    this:hide()
    dlg:show()
    return true
end
```

4. **Add unit tests**: Create `builtin/mainmenu/tests/yourfeature_spec.lua`
```lua
describe("your feature dialog", function()
    it("creates without error", function()
        local dlg = create_yourfeature_dialog()
        assert.is_not_nil(dlg)
    end)
end)
```

---

## 13. Common Patterns & Pitfalls

### Table Data — Escape Each Cell Individually

**WRONG** (escapes commas, breaks column layout):
```lua
row = core.formspec_escape(username .. "," .. pubkey)
```

**CORRECT** (escape each cell, commas stay unescaped):
```lua
row = core.formspec_escape(username) .. "," .. core.formspec_escape(pubkey)
```

Or use a helper:
```lua
local function escape_row(cells)
    local escaped = {}
    for i, cell in ipairs(cells) do
        escaped[i] = core.formspec_escape(tostring(cell))
    end
    return table.concat(escaped, ",")
end
```

### Table Columns — Comma vs Semicolon

- **Semicolons** separate columns: `text,align=left;width=3;text,align=left;width=7`
  means two columns: `text,align=left,width=3` and `text,align=left,width=7`
- **Commas** separate options within a column: `text,align=left,width=3`
  means one column of type `text`, left-aligned, width 3

**WRONG** (creates phantom columns):
```
tablecolumns[text,align=left;width=3;text,align=left;width=7]
-- Parser sees 4 columns: text,align=left | width=3 | text,align=left | width=7
```

**CORRECT**:
```
tablecolumns[text,align=left,width=3;text,align=left,width=7]
-- Parser sees 2 columns with options
```

### Dynamic Layout — Compute Y Top-to-Bottom

Instead of hard-coding Y positions that may overlap, use incremental Y:

```lua
local y = 0.5
local title_y = y;  y = y + 0.7
local status_y = y; y = y + 0.8
local table_y = y;  y = y + table_height + 1.0
-- ... etc
```

### fgettext() for Translatable Strings

Always use `fgettext("English text")` or `fgettext("Text with $1", var)` for
user-visible strings. Do not concatenate translated fragments — use `$1`, `$2`
placeholders instead.

### Container Scoping

Use `container[X,Y] ... container_end[]` to group elements with relative
positioning. This makes it easy to move entire sections by changing one Y
value.

---

## Quick Reference: File → Purpose Map

| File | What It Controls |
|------|-----------------|
| `builtin/mainmenu/init.lua` | Tab order, layout constants, module loading |
| `builtin/mainmenu/tab_local.lua` | Start Game tab |
| `builtin/mainmenu/tab_online.lua` | Join Game tab, server browser, login area |
| `builtin/mainmenu/tab_content.lua` | Content tab, installed packages |
| `builtin/mainmenu/tab_about.lua` | About tab, credits |
| `builtin/mainmenu/dlg_keypair_manager.lua` | Keypair management dialog |
| `builtin/mainmenu/dlg_register.lua` | Server registration dialog |
| `builtin/mainmenu/dlg_create_world.lua` | World creation dialog |
| `builtin/mainmenu/dlg_config_world.lua` | World mod configuration |
| `builtin/mainmenu/dlg_confirm_exit.lua` | Exit confirmation |
| `builtin/mainmenu/dlg_version_info.lua` | Version update notification |
| `builtin/mainmenu/dlg_clients_list.lua` | Connected players list |
| `builtin/mainmenu/dlg_server_list_mods.lua` | Server mods list |
| `builtin/mainmenu/common.lua` | Shared helpers, server list row rendering |
| `builtin/common/menu.lua` | Color constants (`mt_color_*`) |
| `builtin/mainmenu/game_theme.lua` | Backgrounds, sky colors, music |
| `builtin/common/settings/dlg_settings.lua` | Settings dialog |
| `builtin/game/death_screen.lua` | Death screen |
| `builtin/game/hud.lua` | HUD elements (health, breath, minimap, hotbar) |
| `src/gui/guiFormSpecMenu.cpp` | Formspec parser (all elements) |
| `src/gui/StyleSpec.h` | Style property definitions |
| `src/gui/guiTable.cpp` | Table widget rendering |
| `src/gui/guiButton.cpp` | Button widget rendering |
| `src/script/lua_api/l_mainmenu.cpp` | Lua API bindings for mainmenu |
| `textures/base/pack/` | Menu texture images |
| `fonts/` | Font files (Arimo, Cousine, DroidSansFallback) |
