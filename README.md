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

### ✨ Preview Support

| Type | Formats | Status |
|------|---------|--------|
| **Images** | JPEG, PNG, BMP, SVG, WEBP, ICO | ✅ Built-in |
| **Animated GIFs** | GIF animations with proper timing | ✅ Built-in |
| **Text Files** | TXT, MD, LOG, JSON, XML, YAML, code files | ✅ Built-in |
| **Videos** | MP4, MKV, AVI, WEBM, etc. with playback controls | ✅ Optional (GStreamer) |
| **Audio** | MP3, FLAC, OGG, WAV, etc. with playback controls | ✅ Optional (GStreamer) |
| **PDFs** | First page rendering | ✅ Optional (Poppler) |

### 🎨 User Experience

- **Enabled by Default**: Preview pane opens automatically - no setup required
- **Live Resize**: Previews instantly resize as you drag the divider
- **F7 Keyboard Shortcut**: Toggle preview pane on/off
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

### 🚀 Quick Install (Recommended)

Download and run the interactive installer:

```bash
wget https://github.com/chesterbait88/icarus-fm/releases/latest/download/install-icarus-fm.sh
chmod +x install-icarus-fm.sh
./install-icarus-fm.sh
```

The installer will:
1. ✅ Detect your system
2. ✅ Let you choose which features to enable
3. ✅ Install required dependencies
4. ✅ Download and install the package
5. ✅ Launch Icarus-FM

### 📦 Manual Installation

Download the `.deb` files from the [latest release](https://github.com/chesterbait88/icarus-fm/releases/latest):

```bash
# Install the packages
sudo dpkg -i icarus-fm_*.deb icarus-fm-data_*.deb libicarus-fm-extension1_*.deb gir1.2-icarus-fm-3.0_*.deb

# Fix any missing dependencies
sudo apt-get install -f

# Optional: Enable video/audio preview
sudo apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-gtk3

# Optional: Enable PDF preview
sudo apt-get install libpoppler-glib8

# Launch Icarus-FM
icarus-fm
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

## System Requirements

- **OS**: Debian 11+, Ubuntu 20.04+, or Linux Mint 20+
- **Desktop**: Cinnamon (recommended) or any DE using Nemo
- **Architecture**: x86_64 / amd64

### Optional Dependencies

| Feature | Package | Size |
|---------|---------|------|
| Video/Audio | `gstreamer1.0-*` | ~15 MB |
| PDF Preview | `libpoppler-glib8` | ~2 MB |

## Building from Source

See [BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md) for detailed instructions.

### Quick Build

```bash
# Install build dependencies
sudo apt-get build-dep nemo
sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libpoppler-glib-dev

# Clone and build
git clone https://github.com/chesterbait88/icarus-fm.git
cd icarus-fm
git checkout master

# Build .deb package
dpkg-buildpackage -us -uc -b

# Install
cd ..
sudo dpkg -i icarus-fm_*.deb icarus-fm-data_*.deb libicarus-fm-extension1_*.deb gir1.2-icarus-fm-3.0_*.deb
```

## Uninstalling

To remove Icarus-FM:

```bash
sudo apt-get remove icarus-fm
```

Your settings and files are not affected. Your system Nemo file manager will continue to work normally.

## Development

### Project Structure

```
icarus-fm/
├── src/
│   ├── nemo-window-slot.c    # Main preview pane implementation (~800 lines)
│   ├── nemo-window-slot.h    # Preview widget definitions
│   ├── nemo-window-menus.c   # F7 toggle action
│   └── nemo-main.c            # GStreamer initialization
├── debian/                    # Debian packaging
├── docs/                      # Documentation
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
- **Preview pane enabled by default** - No more going to settings to enable it
- **Live resize** - Previews instantly re-render when you drag the divider
- **Taskbar integration** - Pins to taskbar as its own app, not confused with system Nemo
- **Unique application ID** - `org.IcarusFM` for proper desktop integration

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

**Made with ❤️ for the Linux community**

[Report Bug](https://github.com/chesterbait88/icarus-fm/issues) • [Request Feature](https://github.com/chesterbait88/icarus-fm/issues)

</div>
