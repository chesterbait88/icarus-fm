# Icarus-FM

<div align="center">

**Independent file manager with Windows Explorer-style preview pane**

[Features](#features) • [Installation](#installation) • [Usage](#usage) • [Building](#building) • [Credits](#credits)

![License](https://img.shields.io/badge/license-GPL--2.0-blue)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey)
![Maintained](https://img.shields.io/badge/maintained-yes-green)

</div>

---

## About

**Icarus-FM** is an independent file manager with a resizable preview pane, similar to Windows Explorer. Select any file and see a live preview on the right side of the window.

**IMPORTANT**: Icarus-FM is completely independent and does NOT conflict with system file managers like Nemo. It can be safely installed alongside other file managers without breaking your Cinnamon Desktop.

Based on Nemo 6.6.1 codebase with ~800 lines of new preview code, fully rebranded to avoid any conflicts.

## Features

### Preview Support

| Type | Formats | Status |
|------|---------|--------|
| **Images** | JPEG, PNG, BMP, SVG, WEBP, ICO | Built-in |
| **Animated GIFs** | GIF animations with proper timing | Built-in |
| **Text Files** | TXT, MD, LOG, JSON, XML, YAML, code files | Built-in |
| **Videos** | MP4, MKV, AVI, WEBM, etc. with playback controls | Optional (GStreamer) |
| **Audio** | MP3, FLAC, OGG, WAV, etc. with playback controls | Optional (GStreamer) |
| **PDFs** | First page rendering | Optional (Poppler) |

### User Experience

- **Enabled by Default**: Preview pane opens automatically - no setup required
- **Live Resize**: Previews instantly resize as you drag the divider
- **F7 Keyboard Shortcut**: Toggle preview pane on/off
- **Nemo Extensions Work**: Compatible with existing C-based Nemo extensions
- **Independent Identity**: Pins to taskbar separately from system Nemo
- **Smooth Integration**: Feels native, not a bolt-on
- **Graceful Fallback**: Works even without optional dependencies

## Screenshots

<!-- TODO: Add screenshots here showing:
- Preview pane with an image
- Preview pane with video playback
- Preview pane with PDF
- Preview pane with text file
-->

*Screenshots coming soon - or add your own!*

## Installation

### Quick Install (Recommended)

Download and run the interactive installer:

```bash
wget https://raw.githubusercontent.com/chesterbait88/icarus-fm/master/install-icarus-fm.sh
chmod +x install-icarus-fm.sh
./install-icarus-fm.sh
```

The installer will:
1. Detect your system (Debian, Ubuntu, Linux Mint)
2. Let you choose which preview features to enable (video, audio, PDF)
3. Install all required build dependencies
4. Download and compile from source
5. Install to your system
6. Create an uninstall script
7. Launch Icarus-FM

### Manual Installation (Build from Source)

```bash
# Install build dependencies
sudo apt install meson ninja-build libgtk-3-dev libglib2.0-dev \
  libcinnamon-desktop-dev libxapp-dev libnotify-dev libexempi-dev \
  libexif-dev libxml2-dev gobject-introspection libgirepository1.0-dev

# Clone the repository
git clone https://github.com/chesterbait88/icarus-fm.git
cd icarus-fm

# Build
meson setup builddir
ninja -C builddir

# Install
sudo ninja -C builddir install

# Launch
icarus-fm
```

### Optional Dependencies

Install these for additional preview features:

```bash
# Video and Audio preview (with playback controls)
sudo apt install gstreamer1.0-plugins-good gstreamer1.0-gtk3 gstreamer1.0-libav

# PDF preview
sudo apt install libpoppler-glib8
```

## Usage

### Toggle Preview Pane

- **Keyboard**: Press `F7`
- **Menu**: View → Preview Pane

### Resize Preview

Drag the vertical divider between the file list and preview pane.

### Supported File Types

The preview pane automatically detects file types and shows appropriate previews:

- **Images**: Automatically scaled to fit
- **Text**: Syntax-aware with monospace font
- **Videos/Audio**: Click play button to start playback, use timeline to seek
- **PDFs**: First page rendered as image
- **Others**: Shows file icon

## Uninstalling

### If installed with the installer script:

```bash
sudo icarus-fm-uninstall.sh
```

### If installed manually with ninja:

```bash
cd icarus-fm
sudo ninja -C builddir uninstall
```

Your settings and files are not affected. Your system Nemo file manager will continue to work normally.

## System Requirements

- **OS**: Debian 11+, Ubuntu 20.04+, or Linux Mint 20+
- **Desktop**: Cinnamon (recommended) or any GTK-based desktop environment
- **Architecture**: x86_64 / amd64

### Optional Dependencies

| Feature | Package | Size |
|---------|---------|------|
| Video/Audio | `gstreamer1.0-*` | ~15 MB |
| PDF Preview | `libpoppler-glib8` | ~2 MB |

## Development

### Project Structure

```
icarus-fm/
├── src/
│   ├── nemo-window-slot.c    # Main preview pane implementation (~800 lines)
│   ├── nemo-window-slot.h    # Preview widget definitions
│   ├── nemo-window-menus.c   # F7 toggle action
│   └── nemo-main.c           # GStreamer initialization
├── libnemo-private/          # Core library
│   └── org.icarus-fm.gschema.xml  # GSettings schema
├── gresources/               # UI resources
│   └── nemo-shell-ui.xml     # Menu definitions
├── docs/                     # Documentation
│   ├── PREVIEW_PANE_CHANGELOG.md
│   └── video-preview-setup.md
└── install-icarus-fm.sh      # Interactive installer
```

### Contributing

Contributions welcome! Areas for improvement:

- [ ] Multi-page PDF navigation
- [ ] EXIF data display for photos
- [ ] Audio metadata display (artist, album, cover art)
- [ ] Video thumbnail generation for unsupported formats
- [ ] Remember preview pane size in GSettings
- [ ] Async image loading for huge files

## Credits

### Original Nemo

- **Developers**: Linux Mint team
- **Repository**: https://github.com/linuxmint/nemo
- **License**: GPL-2.0

### Icarus-FM (Preview Pane + Rebrand)

- **Developer**: chesterbait88
- **Repository**: https://github.com/chesterbait88/icarus-fm
- **License**: GPL-2.0 (same as Nemo)

### Built With

- **GTK 3**: UI toolkit
- **GStreamer**: Video/audio playback (optional)
- **Poppler**: PDF rendering (optional)
- **GdkPixbuf**: Image loading and animated GIF support

## License

GPL-2.0 - same as Nemo

This is a derivative work of Nemo, which is licensed under GPL-2.0. All changes are released under the same license.

## Changelog

See [docs/PREVIEW_PANE_CHANGELOG.md](docs/PREVIEW_PANE_CHANGELOG.md) for detailed development history.

### Latest Release: v1.1.0

**New in v1.1.0:**
- **Nemo extensions now work!** - Compatible C-based extensions load automatically
- **Preview pane enabled by default** - No more going to settings to enable it
- **Live resize** - Previews instantly re-render when you drag the divider
- **Taskbar integration** - Pins to taskbar as its own app, not confused with system Nemo
- **Complete branding separation** - Independent GSettings, config directories, and D-Bus paths
- **Build from source installer** - No more .deb packages, always get the latest code

**v1.0.0:**
- Complete rebrand from Nemo to Icarus-FM
- Completely independent - does NOT conflict with system Nemo
- Can be safely installed alongside other file managers
- Safe for Cinnamon Desktop - will not break your system
- Full preview pane with support for images, text, video, audio, PDF
- F7 keyboard shortcut to toggle preview
- Resizable preview pane
- Optional dependencies with graceful fallback

---

<div align="center">

**Made with love for the Linux community**

[Report Bug](https://github.com/chesterbait88/icarus-fm/issues) • [Request Feature](https://github.com/chesterbait88/icarus-fm/issues)

</div>
