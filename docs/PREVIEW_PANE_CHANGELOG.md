# Icarus-FM - Development Changelog

## Overview

This document chronicles the development of Icarus-FM, an independent file manager with a Windows Explorer-style preview pane. Based on Nemo 6.6.1, fully rebranded to install alongside system file managers without conflicts.

**Repository:** https://github.com/chesterbait88/icarus-fm
**Base:** Nemo 6.6.1 (Linux Mint)

---

## Version 1.1.0 (Latest)

### New Features

| Feature | Description |
|---------|-------------|
| **Preview Enabled by Default** | Preview pane now opens automatically when launching Icarus-FM |
| **Live Resize** | Previews instantly re-render when dragging the pane divider |
| **Taskbar Integration** | Properly pins to taskbar as separate app from system Nemo |
| **Unique Application ID** | Changed from `org.Nemo` to `org.IcarusFM` |

### Technical Changes

- Added `StartupWMClass=icarus-fm` to desktop file for proper window matching
- Changed `g_set_prgname("nemo")` → `g_set_prgname("icarus-fm")` for correct WM_CLASS
- Changed GApplication IDs: `org.Nemo` → `org.IcarusFM`, `org.NemoDesktop` → `org.IcarusFMDesktop`
- Added `size-allocate` signal handler on preview pane for live resize
- Changed default `preview_visible` from FALSE to TRUE
- Added `last_preview_width` field to NemoWindowSlot for resize detection

### Files Modified in v1.1.0

| File | Changes |
|------|---------|
| `src/nemo-window-slot.c` | Added resize callback, enabled preview by default |
| `src/nemo-window-slot.h` | Added `last_preview_width` field |
| `src/nemo-window-menus.c` | Changed preview toggle default to TRUE |
| `src/nemo-main.c` | Changed prgname to `icarus-fm` |
| `src/nemo-desktop-main.c` | Changed prgname to `icarus-fm-desktop` |
| `src/nemo-main-application.c` | Changed application-id to `org.IcarusFM` |
| `src/nemo-desktop-application.c` | Changed application-id to `org.IcarusFMDesktop` |
| `data/nemo.desktop.in` | Added `StartupWMClass=icarus-fm` |

---

## Version 1.0.0

### Complete Rebrand

Renamed from "nemo-preview" to "Icarus-FM" - a completely independent file manager that can be installed alongside system Nemo without conflicts.

### Rebranding Changes

| Component | Before | After |
|-----------|--------|-------|
| Binary | `nemo` | `icarus-fm` |
| Desktop Binary | `nemo-desktop` | `icarus-fm-desktop` |
| Library | `libnemo-extension` | `libicarus-fm-extension` |
| GIR Namespace | `Nemo` | `IcarusFM` |
| GSettings Schema | `org.nemo.*` | `org.icarus-fm.*` |
| Data Path | `/usr/share/nemo` | `/usr/share/icarus-fm` |
| DBus Service | `org.Nemo` | `org.IcarusFM` |

---

## Features Implemented

| Feature | Status | Description |
|---------|--------|-------------|
| Image Preview | Complete | JPEG, PNG, BMP, SVG, WEBP, etc. via GdkPixbuf |
| Animated GIF | Complete | Frame-by-frame animation with proper timing |
| Text Preview | Complete | TXT, MD, LOG, JSON, XML, code files (monospace, read-only) |
| Video Preview | Complete | Full playback with play/pause and timeline |
| Audio Preview | Complete | Simple player with play/pause and timeline |
| PDF Preview | Complete | First page rendered via Poppler library |
| Error Handling | Complete | Graceful fallbacks with setup instructions |

**Keyboard Shortcut:** `F7` toggles the preview pane
**Menu:** View > Preview Pane

---

## Technical Implementation

### Architecture

The preview pane is implemented at the `NemoWindowSlot` level, wrapping the existing content view in a `GtkPaned` widget:

```
NemoWindowSlot
├── extra_location_widgets
├── GtkPaned (content_paned) ← NEW
│   ├── child1: view_overlay (existing file list)
│   └── child2: preview_pane ← NEW
│       ├── Image scroll + GtkImage
│       ├── Text scroll + GtkTextView
│       ├── Video box + controls
│       ├── Audio box + controls
│       └── PDF scroll + GtkImage
└── floating_bar
```

### Files Modified

| File | Changes |
|------|---------|
| `src/nemo-window-slot.h` | Added preview pane widget fields (~40 lines) |
| `src/nemo-window-slot.c` | Added ~800 lines for preview functionality |
| `src/nemo-window-menus.c` | Added preview pane toggle callback and menu entry |
| `src/nemo-actions.h` | Added `NEMO_ACTION_SHOW_HIDE_PREVIEW_PANE` |
| `src/nemo-main.c` | Added GStreamer initialization |
| `gresources/nemo-shell-ui.xml` | Added preview pane menu item |
| `meson.build` | Added GStreamer and Poppler dependencies |
| `src/meson.build` | Added conditional dependencies |
| `config.h.meson.in` | Added `HAVE_GSTREAMER` and `HAVE_POPPLER` defines |
| `docs/meson.build` | Added video-preview-setup.md installation |

### New Dependencies (Optional)

All dependencies are configured with `required: false` - features gracefully disable if not available:

```bash
# Video/Audio preview (GStreamer)
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install gstreamer1.0-plugins-good gstreamer1.0-gtk3

# PDF preview (Poppler)
sudo apt install libpoppler-glib-dev
```

---

## Development Journey

### Phase 1: Research & Planning

**Challenge:** Determining whether this could be built as an extension or required core modification.

**Discovery:** Nemo's extension API (`libnemo-extension`) only supports:
- Context menu items
- Property pages
- File info providers
- Column providers
- Location widgets (above file list)

It **cannot** add persistent panes or modify window layout. The "nemo-preview" extension is actually a separate DBus service that spawns a floating window - not truly embedded.

**Decision:** Implement as a core Nemo modification rather than an extension.

### Phase 2: Image Preview (Foundation)

**Implementation:**
1. Added struct fields to `NemoWindowSlot` for preview widgets
2. Modified `nemo_window_slot_init()` to wrap content view in `GtkPaned`
3. Created `create_preview_pane()` function building the widget hierarchy
4. Connected to `selection-changed` signal from `NemoView`
5. Implemented `update_preview_for_file()` using `gdk_pixbuf_new_from_file_at_scale()`

**Challenge:** Running the dev build crashed with GSettings schema errors.

**Solution:** The dev build expects schema keys not in the system-installed schemas. Fixed by compiling local schemas:
```bash
mkdir -p build/gschemas
cp libnemo-private/org.nemo.gschema.xml build/gschemas/
glib-compile-schemas build/gschemas/
GSETTINGS_SCHEMA_DIR=./build/gschemas ./build/src/nemo
```

### Phase 3: Text Preview

**Implementation:**
1. Added `is_text_mime_type()` function supporting `text/*` and common application types (JSON, XML, shell scripts, etc.)
2. Created `GtkTextView` widget (monospace font, read-only, word wrap)
3. Implemented file loading with UTF-8 validation and 1MB truncation limit

**Supported types:** Plain text, Markdown, logs, JSON, XML, JavaScript, Python, Shell, YAML, TOML, SQL, and more.

### Phase 4: Video Preview

**Challenge:** GStreamer integration is complex - requires proper initialization and widget embedding.

**Implementation:**
1. Added GStreamer dependencies with `required: false` to handle systems without it
2. Created `playbin` pipeline with `gtksink` for GTK widget embedding
3. Implemented play/pause toggle and timeline slider
4. Added bus message handler for EOS (loop) and errors

**Critical Bug #1:** Video preview not working despite code being present.
- **Cause:** `#mesondefine HAVE_GSTREAMER` was missing from `config.h.meson.in`
- **Fix:** Added the mesondefine so the preprocessor directive actually gets generated

**Critical Bug #2:** Still not working after mesondefine fix.
- **Cause:** `gst_init()` was never called - GStreamer requires initialization before any API calls
- **Fix:** Added `gst_init(&argc, &argv)` in `nemo-main.c` with `#ifdef HAVE_GSTREAMER`

**Critical Bug #3:** Seeking caused infinite loop.
- **Cause:** `update_video_timeline()` was blocking the wrong signal handler (`gtk_range_get_value` instead of `on_timeline_value_changed`)
- **Fix:** Corrected the signal handler name in blocking/unblocking calls

### Phase 5: Error Handling & User Experience

**User requirement:** "Make sure it won't kill someone's file explorer"

**Implementation:**
1. Added comprehensive NULL checks throughout all preview functions
2. Created `show_video_fallback_icon()` for graceful degradation
3. Added error UI with helpful message: "Video preview unavailable. Required GStreamer plugins not found."
4. Added clickable link to setup instructions
5. Created `docs/video-preview-setup.md` with installation commands for Ubuntu/Fedora/Arch

**User requirement:** Make help link open local file instead of GitHub URL

**Implementation:**
1. Added `NEMO_DOCDIR` compile-time define in `meson.build`
2. Configured doc file installation via `docs/meson.build`
3. Modified `on_video_help_link_clicked()` to build local path and open with `gtk_show_uri_on_window()`

### Phase 6: Animated GIF Support

**Implementation:**
1. Used `GdkPixbufAnimation` (no new dependencies)
2. Created timer-based frame updates using `g_timeout_add()` with frame delay from iterator
3. Added `stop_gif_animation()` called when switching files or closing pane

**Key functions:**
- `gdk_pixbuf_animation_new_from_file()` - Load animation
- `gdk_pixbuf_animation_is_static_image()` - Detect if truly animated
- `gdk_pixbuf_animation_iter_get_delay_time()` - Get frame timing

### Phase 7: PDF Preview

**Implementation:**
1. Added Poppler dependency with `required: false`
2. Load PDF with `poppler_document_new_from_file()`
3. Get first page and render to Cairo surface
4. Scale to fit preview pane width
5. Convert Cairo surface to GdkPixbuf for display

**Error handling:** Password-protected and corrupted PDFs show a PDF icon fallback.

### Phase 8: Audio Preview

**User clarification:** "I don't want metadata display - just a simple player like video"

**Implementation:**
1. Reused GStreamer `playbin` architecture (no video sink)
2. Added 96px audio icon (`audio-x-generic`)
3. Implemented play/pause and timeline controls (same as video)
4. Separate pipeline from video (`audio_pipeline` vs `video_pipeline`)

**Challenge:** Forward declaration ordering caused build errors.
- `stop_audio_pipeline()` was called in `show_image_preview()` before being declared
- **Fix:** Added forward declaration before first usage, removed duplicate declaration that caused "static declaration follows non-static" error

---

## Installation

### Build from Source

```bash
# Install dependencies
sudo apt install meson ninja-build libgtk-3-dev libglib2.0-dev \
    libcinnamon-desktop-dev libxapp-dev libexempi-dev libexif-dev \
    gobject-introspection libgirepository1.0-dev

# Optional: Video/Audio preview
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good gstreamer1.0-gtk3

# Optional: PDF preview
sudo apt install libpoppler-glib-dev

# Configure and build
cd /path/to/nemo
meson setup build --prefix=/usr
ninja -C build

# Install (replaces system nemo)
sudo ninja -C build install
```

### Testing Without Install

```bash
# Compile local schemas
mkdir -p build/gschemas
cp libnemo-private/org.nemo.gschema.xml build/gschemas/
glib-compile-schemas build/gschemas/

# Run dev build
GSETTINGS_SCHEMA_DIR=./build/gschemas ./build/src/nemo
```

---

## Known Issues

### Toolbar Icons Missing After Installation

**Status:** Unresolved

After installing to `/usr`, toolbar icons display as blank squares. Investigation confirmed:
- All icon files ARE physically installed correctly
- Schema compilation successful
- Icon cache updated
- Process restart and logout/login attempted

The issue appears to be related to the `xsi-` prefixed icons (XApp Symbolic Icons) which Nemo's toolbar uses. These icons come from the `xapp-symbolic-icons` package which may need to be installed separately:

```bash
sudo apt install xapp-symbolic-icons
sudo gtk-update-icon-cache -f /usr/share/icons/hicolor/
```

---

## Lessons Learned

1. **GStreamer requires explicit initialization** - `gst_init()` must be called before any GStreamer API functions work.

2. **Meson defines need `#mesondefine`** - Just setting `conf.set('HAVE_FEATURE', true)` isn't enough; you need the corresponding `#mesondefine HAVE_FEATURE` in `config.h.meson.in`.

3. **Signal handler blocking must use correct function** - When preventing feedback loops during programmatic updates, ensure you're blocking the actual callback function, not a different function.

4. **Forward declarations must precede usage** - C requires functions to be declared before they're called, even within `#ifdef` blocks.

5. **Extension API has limits** - Research the extension API capabilities early; don't assume you can add any UI element as an extension.

6. **Each show function must handle ALL preview types** - When adding a new preview type, update ALL existing `show_*_preview()` functions to hide the new widget and stop any associated pipelines/timers.

7. **Optional dependencies with graceful fallback** - Use `required: false` in meson for optional features, and wrap code in `#ifdef HAVE_*` blocks with helpful fallback UI.

---

## File Reference

### New Files Created
- `docs/video-preview-setup.md` - User documentation for GStreamer setup
- `docs/PREVIEW_PANE_CHANGELOG.md` - This file

### Configuration Files Modified
- `meson.build` - GStreamer/Poppler deps, NEMO_DOCDIR define
- `src/meson.build` - Conditional dependency linking
- `config.h.meson.in` - Feature detection defines
- `docs/meson.build` - Documentation installation

### Source Files Modified
- `src/nemo-window-slot.h` - Preview widget fields
- `src/nemo-window-slot.c` - All preview functionality (~800 lines)
- `src/nemo-window-menus.c` - Menu toggle
- `src/nemo-actions.h` - Action define
- `src/nemo-main.c` - GStreamer init
- `gresources/nemo-shell-ui.xml` - Menu XML

---

## Future Enhancements

- [ ] EXIF data display for photos
- [ ] Multi-page PDF navigation
- [ ] Remember pane position in GSettings
- [ ] Async image loading for large files
- [ ] Video thumbnail generation for non-playable formats
