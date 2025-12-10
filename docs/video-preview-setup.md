# Video and Audio Preview Setup

Nemo Preview includes optional support for video and audio file previews with playback controls. This feature requires GStreamer libraries to be installed on your system.

## Quick Setup

### Ubuntu / Debian / Linux Mint

```bash
sudo apt-get update
sudo apt-get install \
    libgstreamer1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-gtk3 \
    gstreamer1.0-libav
```

### Fedora

```bash
sudo dnf install \
    gstreamer1 \
    gstreamer1-plugins-base \
    gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free \
    gstreamer1-libav
```

### Arch Linux

```bash
sudo pacman -S \
    gstreamer \
    gst-plugins-base \
    gst-plugins-good \
    gst-plugins-bad \
    gst-libav
```

## What Gets Installed

- **gstreamer1.0-0** / **gstreamer1**: Core GStreamer framework
- **gstreamer1.0-plugins-base**: Basic codecs (Ogg, Vorbis, Theora)
- **gstreamer1.0-plugins-good**: Good quality codecs (MP3, PNG, JPEG)
- **gstreamer1.0-plugins-bad**: Additional codecs (may be less stable)
- **gstreamer1.0-gtk3**: GTK integration for embedding video
- **gstreamer1.0-libav**: FFmpeg-based codecs (H.264, AAC, etc.)

Total size: ~15-20 MB

## Supported Formats

Once installed, the preview pane will support:

### Video
- MP4 (H.264, H.265)
- MKV
- AVI
- WEBM
- MOV
- FLV
- OGV

### Audio
- MP3
- FLAC
- OGG / Vorbis
- WAV
- AAC
- M4A
- OPUS

## Features

- **Play/Pause**: Click the play button to start/stop playback
- **Timeline Scrubbing**: Drag the timeline slider to seek
- **Auto-pause**: Playback stops when you switch files
- **Volume Control**: Uses system volume

## Troubleshooting

### Video preview shows error message

If you see "Video preview unavailable. Required GStreamer plugins not found":

1. Install the packages above
2. Restart Nemo:
   ```bash
   pkill -9 nemo
   nemo
   ```

### Specific video format doesn't work

Some formats require additional codec packages:

```bash
# For H.265/HEVC support
sudo apt-get install gstreamer1.0-plugins-bad

# For proprietary formats
sudo apt-get install gstreamer1.0-libav

# For DVD support
sudo apt-get install gstreamer1.0-plugins-ugly libdvdread8
```

### Audio but no video

This usually means missing video codec. Install:

```bash
sudo apt-get install gstreamer1.0-libav gstreamer1.0-plugins-bad
```

### No audio or video

Check that GStreamer is properly installed:

```bash
# Test audio
gst-launch-1.0 audiotestsrc ! autoaudiosink

# Test video
gst-launch-1.0 videotestsrc ! autovideosink
```

If these don't work, reinstall GStreamer:

```bash
sudo apt-get install --reinstall gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

## Disabling Video Preview

If you don't want video preview (to save resources or disk space):

1. Don't install GStreamer packages
2. Or uninstall them:
   ```bash
   sudo apt-get remove gstreamer1.0-plugins-*
   ```

Nemo Preview will continue to work for images, text, and PDFs.

## Performance Notes

- Video preview uses hardware acceleration when available
- Large video files may take a moment to load the first frame
- Playback quality matches the original file
- Memory usage is minimal (only active when preview pane is open)

## PDF Preview Setup

For PDF preview support, install Poppler:

```bash
sudo apt-get install libpoppler-glib8
```

This is much smaller (~2 MB) and shows the first page of PDF documents.
