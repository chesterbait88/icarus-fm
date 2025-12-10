# Building and Releasing Icarus-FM

This guide explains how to build the `.deb` package and create a GitHub release for distribution.

**IMPORTANT**: Icarus-FM is completely independent and does NOT conflict with system Nemo.

## Prerequisites

### Install Build Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    debhelper \
    devscripts \
    dh-python \
    meson \
    ninja-build \
    cinnamon-l10n \
    gobject-introspection \
    gtk-doc-tools \
    intltool \
    itstool \
    libatk1.0-dev \
    libcinnamon-desktop-dev \
    libexempi-dev \
    libexif-dev \
    libgail-3-dev \
    libgirepository1.0-dev \
    libglib2.0-dev \
    libglib2.0-doc \
    libgsf-1-dev \
    libgtk-3-dev \
    libgtk-3-doc \
    libjson-glib-dev \
    libpango1.0-dev \
    libx11-dev \
    libxapp-dev \
    libxext-dev \
    libxrender-dev \
    python3 \
    python3-gi \
    x11proto-core-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libpoppler-glib-dev
```

## Building the .deb Package

### Method 1: Using dpkg-buildpackage (Recommended)

This is the standard Debian way to build packages:

```bash
# 1. Ensure you're on the master branch
git checkout master

# 2. Clean any previous builds
rm -rf debian/tmp debian/.debhelper debian/files
rm -f ../*.deb ../*.buildinfo ../*.changes

# 3. Build the package
dpkg-buildpackage -us -uc -b

# 4. The .deb files will be created in the parent directory
ls -lh ../*.deb
```

**Expected output:**
- `icarus-fm_1.0.0_amd64.deb` - Main package (this is what users install)
- `libicarus-fm-extension1_1.0.0_amd64.deb` - Library package
- `libicarus-fm-extension-dev_1.0.0_amd64.deb` - Development headers
- `gir1.2-icarus-fm-3.0_1.0.0_amd64.deb` - GObject introspection
- `icarus-fm-data_1.0.0_all.deb` - Data files
- `icarus-fm-dbg_1.0.0_amd64.deb` - Debug symbols

### Method 2: Using meson directly (for testing)

If you just want to test the build without creating packages:

```bash
# Configure
meson setup build --prefix=/usr

# Build
ninja -C build

# Install locally (for testing)
sudo ninja -C build install

# Launch icarus-fm
icarus-fm
```

## Testing the Package

Before releasing, test the package on a clean system or VM:

```bash
# Install the package
sudo dpkg -i icarus-fm_1.0.0_amd64.deb \
              icarus-fm-data_1.0.0_all.deb \
              libicarus-fm-extension1_1.0.0_amd64.deb \
              gir1.2-icarus-fm-3.0_1.0.0_amd64.deb

# Fix any missing dependencies
sudo apt-get install -f

# Test basic functionality
icarus-fm

# Test with optional dependencies
sudo apt-get install \
    libgstreamer1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-gtk3 \
    libpoppler-glib8

# Test preview features:
# - Open Icarus-FM
# - Press F7 to show preview pane
# - Select various file types (images, videos, PDFs, text files)
# - Verify each preview type works
```

## Creating a GitHub Release

### 1. Push Your Changes to GitHub

```bash
# Add all modified files
git add debian/control \
        debian/changelog \
        debian/icarus-fm.install \
        debian/icarus-fm.lintian-overrides \
        install-icarus-fm.sh \
        BUILD_AND_RELEASE.md \
        docs/PREVIEW_PANE_CHANGELOG.md \
        README.md

# Also add all your source code changes
git add src/ \
        meson.build \
        config.h.meson.in \
        gresources/

# Commit
git commit -m "Complete rebrand to Icarus-FM - independent file manager

- Renamed from nemo-preview to Icarus-FM
- Completely independent - does NOT conflict with system Nemo
- Windows Explorer-style preview pane
- Support for images, text, video, audio, PDF
- Updated all packaging and documentation
- Safe for Cinnamon Desktop
"

# Push to your fork
git push origin master
```

### 2. Create a Tag

```bash
# Create annotated tag
git tag -a v1.0.0 -m "Icarus-FM v1.0.0

Complete rebrand to independent file manager:
- Renamed from nemo-preview to Icarus-FM
- Completely independent - does NOT conflict with system Nemo
- Safe for Cinnamon Desktop - will not break your system
- Full preview pane functionality
- Support for images, text, video, audio, PDF
"

# Push the tag
git push origin v1.0.0
```

### 3. Create Release on GitHub Web Interface

1. Go to https://github.com/chesterbait88/icarus-fm/releases
2. Click "Draft a new release"
3. Select tag: `v1.0.0`
4. Title: `Icarus-FM v1.0.0`
5. Description:

```markdown
# Icarus-FM v1.0.0

Independent file manager with Windows Explorer-style preview pane.

## Important - Completely Safe to Install

✅ **SAFE**: Icarus-FM is completely independent and does NOT conflict with or replace system Nemo. Safe for Cinnamon Desktop!

## Features

- ✨ **Image Preview**: JPEG, PNG, SVG, animated GIFs
- 📝 **Text Preview**: Code, logs, JSON, XML, Markdown
- 🎬 **Video Preview**: Full playback with controls (requires GStreamer)
- 🎵 **Audio Preview**: Playback with controls (requires GStreamer)
- 📄 **PDF Preview**: First page rendering (requires Poppler)
- ⌨️ **F7 Shortcut**: Quick toggle preview pane on/off
- 📐 **Resizable**: Drag divider to adjust preview size

## Installation

### Easy Install (Recommended)

```bash
wget https://github.com/chesterbait88/icarus-fm/releases/download/v1.0.0/install-icarus-fm.sh
chmod +x install-icarus-fm.sh
./install-icarus-fm.sh
```

The installer will:
- Let you choose which features to enable
- Install required dependencies
- Download and install the package
- Launch Icarus-FM

### Manual Install

Download all 4 .deb files below, then:

```bash
sudo dpkg -i icarus-fm_*.deb icarus-fm-data_*.deb libicarus-fm-extension1_*.deb gir1.2-icarus-fm-3.0_*.deb
sudo apt-get install -f  # Fix any missing dependencies

# Optional: Install for video/audio preview
sudo apt-get install libgstreamer1.0-0 gstreamer1.0-plugins-{base,good} gstreamer1.0-gtk3

# Optional: Install for PDF preview
sudo apt-get install libpoppler-glib8

# Launch Icarus-FM
icarus-fm
```

## Usage

- Press **F7** to toggle the preview pane
- Or use **View → Preview Pane** from the menu
- Drag the divider to resize

## System Requirements

- Debian 11+, Ubuntu 20.04+, or Linux Mint 20+
- Cinnamon desktop environment
- x86_64 architecture

## Based On

- Nemo 6.6.1 (Linux Mint)
- [Original Repository](https://github.com/linuxmint/nemo)
```

6. **Upload Files**:
   - Drag and drop these files to the release:
     - `icarus-fm_1.0.0_amd64.deb`
     - `icarus-fm-data_1.0.0_all.deb`
     - `libicarus-fm-extension1_1.0.0_amd64.deb`
     - `gir1.2-icarus-fm-3.0_1.0.0_amd64.deb`
     - `install-icarus-fm.sh` (from repository root)

7. Check "Set as the latest release"
8. Click "Publish release"

## Post-Release

### Create a README.md for GitHub

Create a nice README.md in the repository root with:
- Project description
- Screenshot/GIF of the preview pane in action
- Installation instructions
- Features list
- Credits

### Announce the Release

Consider sharing on:
- r/linux
- r/linuxmint
- r/unixporn
- Linux Mint forums
- Your social media

## Updating for New Releases

When you make changes and want to release a new version:

1. Update version in `debian/changelog`:
   ```bash
   dch -v 1.0.1 "Describe changes here"
   ```

2. Rebuild package:
   ```bash
   dpkg-buildpackage -us -uc -b
   ```

3. Create new tag:
   ```bash
   git tag -a v1.0.1 -m "Release notes"
   git push origin v1.0.1
   ```

4. Create new release on GitHub with updated .deb files

## Troubleshooting Build Issues

### Missing dependencies
```bash
sudo apt-get build-dep nemo
```

### Clean build
```bash
git clean -fdx
rm -rf build debian/tmp
dpkg-buildpackage -us -uc -b
```

### Check package contents
```bash
dpkg -c icarus-fm_1.0.0_amd64.deb
```

### Test package installation
```bash
sudo dpkg -i --dry-run icarus-fm_1.0.0_amd64.deb
```
