# Nemo Preview Pane Implementation Plan

## Goal
Add a resizable image preview pane to Nemo file manager, similar to Windows Explorer's preview pane.

## Architecture Overview

### Current Nemo Window Structure
```
NemoWindow
├── NemoWindowDetails
│   ├── GList *panes (list of NemoWindowPane)
│   ├── NemoWindowPane *active_pane
│   └── GtkWidget *split_view_hpane (GtkPaned for extra pane)
│
└── NemoWindowPane (extends GtkBox)
    ├── GtkNotebook (tabs)
    └── NemoWindowSlot (per tab)
        ├── extra_location_widgets (top bar area)
        ├── view_overlay (main content - file list)
        └── floating_bar (status bar)
```

### Target Structure (with Preview Pane)
```
NemoWindowSlot
├── extra_location_widgets
├── GtkPaned (NEW - horizontal, content + preview)
│   ├── child1: view_overlay (existing file list)
│   └── child2: preview_pane (NEW)
│       ├── GtkImage or GtkPicture (image display)
│       └── GtkLabel (filename/info - optional)
└── floating_bar
```

---

## Implementation Steps

### Phase 1: Core Structure Changes

#### Step 1.1: Modify NemoWindowSlot structure
**File:** `src/nemo-window-slot.h`

Add new fields:
```c
struct NemoWindowSlotDetails {
    // ... existing fields ...

    /* Preview pane */
    GtkWidget *preview_pane;           /* Container for preview content */
    GtkWidget *preview_image;          /* GtkImage widget for display */
    GtkWidget *content_paned;          /* GtkPaned: content + preview */
    gboolean   preview_visible;        /* Toggle state */
};
```

#### Step 1.2: Create preview pane widget
**File:** `src/nemo-window-slot.c`

New functions to add:
- `create_preview_pane()` - Build the preview widget structure
- `nemo_window_slot_set_preview_visible()` - Show/hide preview pane
- `nemo_window_slot_update_preview()` - Update preview for selected file
- `update_preview_for_selection()` - Signal handler for selection changes

#### Step 1.3: Integrate into slot layout
**File:** `src/nemo-window-slot.c`

Modify `nemo_window_slot_constructed()` or layout setup:
- Wrap `view_overlay` in a new `GtkPaned`
- Add preview pane as second child
- Connect selection-changed signal to update preview

---

### Phase 2: Image Loading & Display

#### Step 2.1: Simple GdkPixbuf approach
```c
static void
update_preview_for_file (NemoWindowSlot *slot, NemoFile *file)
{
    const char *uri;
    char *path;
    GdkPixbuf *pixbuf;
    GError *error = NULL;
    int preview_width;

    if (file == NULL || !nemo_file_is_image (file)) {
        gtk_widget_hide (slot->details->preview_pane);
        return;
    }

    uri = nemo_file_get_uri (file);
    path = g_filename_from_uri (uri, NULL, NULL);

    preview_width = gtk_widget_get_allocated_width (slot->details->preview_pane);

    pixbuf = gdk_pixbuf_new_from_file_at_scale (
        path,
        preview_width,
        -1,              /* height: preserve aspect ratio */
        TRUE,            /* preserve aspect ratio */
        &error
    );

    if (pixbuf) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (slot->details->preview_image), pixbuf);
        g_object_unref (pixbuf);
        gtk_widget_show (slot->details->preview_pane);
    }

    g_free (path);
}
```

#### Step 2.2: Connect to selection changes
```c
/* In slot setup */
g_signal_connect_object (
    nemo_view_get_selection_model (slot->content_view),
    "selection-changed",
    G_CALLBACK (on_selection_changed),
    slot,
    0
);
```

---

### Phase 3: UI Integration

#### Step 3.1: Add menu toggle
**File:** `src/nemo-window-menus.c`

Add action:
```c
{ "Preview Pane", NULL, N_("_Preview Pane"), "F7",
  N_("Show or hide the preview pane"),
  G_CALLBACK (action_preview_pane_callback) }
```

#### Step 3.2: Add keyboard shortcut
- F7 or similar to toggle preview pane (matching Windows convention)

#### Step 3.3: GSettings preference
**File:** `libnemo-private/org.nemo.gschema.xml`

Add setting for preview pane default state and position.

---

### Phase 4: Polish & Edge Cases

#### Step 4.1: Handle non-image files
- Show file icon for non-previewable files
- Or hide preview pane entirely
- Consider adding text preview for .txt files later

#### Step 4.2: Handle large images efficiently
- Use `gdk_pixbuf_new_from_file_at_scale()` to load scaled versions
- Implement async loading to avoid UI freezes
- Add loading indicator for large files

#### Step 4.3: Remember pane position
- Save divider position to GSettings
- Restore on window creation

#### Step 4.4: Handle edge cases
- Empty selection: hide preview or show placeholder
- Multiple selection: show count or first item
- Directory selection: show folder icon
- Permission denied: show error icon

---

## Key Files to Modify

| File | Changes |
|------|---------|
| `src/nemo-window-slot.h` | Add preview pane fields to struct |
| `src/nemo-window-slot.c` | Create preview pane, connect signals, update logic |
| `src/nemo-window-menus.c` | Add View menu toggle action |
| `src/nemo-file.c` | Add `nemo_file_is_image()` helper if needed |
| `data/org.nemo.gschema.xml` | Add preview pane settings |
| `src/nemo-window.c` | Potentially add window-level toggle |

---

## Build & Test Commands

```bash
# Configure (first time)
meson setup build --prefix=/usr

# Build
ninja -C build

# Test locally (without install)
./build/src/nemo

# Install (to test system-wide)
sudo ninja -C build install
```

---

## Dependencies

Required packages (Ubuntu/Debian):
```bash
sudo apt install meson ninja-build
sudo apt install libgtk-3-dev libglib2.0-dev
sudo apt install libcinnamon-desktop-dev
sudo apt install libxapp-dev
sudo apt install libnotify-dev
sudo apt install libexempi-dev
sudo apt install libexif-dev
sudo apt install gobject-introspection
sudo apt install libgirepository1.0-dev
```

---

## Success Criteria

1. Preview pane appears on right side of file list
2. Pane is resizable via draggable divider
3. Selecting an image file shows preview
4. Preview updates when selection changes
5. Toggle via menu or keyboard shortcut
6. Pane position is remembered

---

## Future Enhancements (Out of Scope for PoC)

- PDF preview (using poppler)
- Text file preview (first N lines) ✓ IMPLEMENTED
- Video thumbnail preview ✓ IMPLEMENTED (full playback)
- Audio file metadata display
- Animated GIF support
- EXIF data display for photos

---

## Enhancement: PDF Preview (using Poppler)

### Overview
Display the first page of PDF documents as a preview image using the Poppler library.

### Dependencies

**Library:** poppler-glib
```bash
# Ubuntu/Debian/Mint
sudo apt install libpoppler-glib-dev

# Fedora
sudo dnf install poppler-glib-devel

# Arch
sudo pacman -S poppler-glib
```

**Meson dependency:**
```meson
# In meson.build
poppler = dependency('poppler-glib', version: '>=0.18', required: false)
poppler_enabled = poppler.found()
conf.set('HAVE_POPPLER', poppler_enabled)
```

### Implementation

**1. Add to config.h.meson.in:**
```c
#mesondefine HAVE_POPPLER
```

**2. Add to nemo-window-slot.h struct:**
```c
GtkWidget *preview_pdf_scroll;   /* Scrolled window for PDF */
GtkWidget *preview_pdf_image;    /* GtkImage for PDF page render */
```

**3. Add helper function in nemo-window-slot.c:**
```c
#ifdef HAVE_POPPLER
#include <poppler.h>

static gboolean
is_pdf_mime_type (const char *mime_type)
{
    return (g_strcmp0 (mime_type, "application/pdf") == 0);
}

static void
show_pdf_preview (NemoWindowSlot *slot)
{
    gtk_widget_hide (slot->preview_image_scroll);
    gtk_widget_hide (slot->preview_text_scroll);
    gtk_widget_hide (slot->preview_video_box);
    gtk_widget_hide (slot->preview_video_error_box);
    gtk_widget_show (slot->preview_pdf_scroll);
}

static void
load_pdf_preview (NemoWindowSlot *slot, const char *path)
{
    PopplerDocument *document;
    PopplerPage *page;
    GError *error = NULL;
    char *uri;
    double width, height;
    int preview_width, render_width, render_height;
    double scale;
    cairo_surface_t *surface;
    cairo_t *cr;
    GdkPixbuf *pixbuf;

    uri = g_filename_to_uri (path, NULL, &error);
    if (uri == NULL) {
        g_warning ("Could not create URI for PDF: %s", error->message);
        g_error_free (error);
        return;
    }

    document = poppler_document_new_from_file (uri, NULL, &error);
    g_free (uri);

    if (document == NULL) {
        g_warning ("Could not open PDF: %s", error->message);
        g_error_free (error);
        gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_pdf_image),
                                      "application-pdf", GTK_ICON_SIZE_DIALOG);
        return;
    }

    if (poppler_document_get_n_pages (document) == 0) {
        g_object_unref (document);
        gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_pdf_image),
                                      "application-pdf", GTK_ICON_SIZE_DIALOG);
        return;
    }

    /* Get first page */
    page = poppler_document_get_page (document, 0);
    poppler_page_get_size (page, &width, &height);

    /* Scale to fit preview pane width */
    preview_width = gtk_widget_get_allocated_width (slot->preview_pane);
    if (preview_width < PREVIEW_PANE_MIN_WIDTH)
        preview_width = PREVIEW_PANE_DEFAULT_WIDTH;

    scale = (double)(preview_width - 20) / width;
    render_width = (int)(width * scale);
    render_height = (int)(height * scale);

    /* Render to cairo surface */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                          render_width, render_height);
    cr = cairo_create (surface);

    /* White background */
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    cairo_paint (cr);

    /* Scale and render */
    cairo_scale (cr, scale, scale);
    poppler_page_render (page, cr);

    cairo_destroy (cr);
    g_object_unref (page);
    g_object_unref (document);

    /* Convert to pixbuf */
    pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                          render_width, render_height);
    cairo_surface_destroy (surface);

    if (pixbuf != NULL) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_pdf_image), pixbuf);
        g_object_unref (pixbuf);
    }
}
#endif
```

**4. Add to update_preview_for_file():**
```c
#ifdef HAVE_POPPLER
else if (is_pdf_mime_type (mime_type)) {
    show_pdf_preview (slot);
    location = nemo_file_get_location (file);
    path = g_file_get_path (location);
    if (path != NULL) {
        load_pdf_preview (slot, path);
        g_free (path);
    }
    g_object_unref (location);
}
#endif
```

**5. Add widgets in create_preview_pane():**
```c
#ifdef HAVE_POPPLER
{
    GtkWidget *pdf_scroll, *pdf_image;

    pdf_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pdf_scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (pdf_scroll, TRUE);
    slot->preview_pdf_scroll = pdf_scroll;

    pdf_image = gtk_image_new ();
    gtk_widget_set_halign (pdf_image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (pdf_image, GTK_ALIGN_START);
    slot->preview_pdf_image = pdf_image;

    gtk_container_add (GTK_CONTAINER (pdf_scroll), pdf_image);
    gtk_box_pack_start (GTK_BOX (box), pdf_scroll, TRUE, TRUE, 0);
}
#endif
```

### Testing
- Select a PDF file → first page renders as image
- Large PDFs → only first page loaded (fast)
- Password-protected PDFs → shows PDF icon (graceful fallback)
- Corrupted PDFs → shows PDF icon with warning

---

## Enhancement: Audio File Metadata Display

### Overview
Display audio file metadata (artist, album, title, duration, cover art) for audio files.

### Dependencies

**Library:** GStreamer (already added) + taglib or use GStreamer's tag reading
```bash
# Using GStreamer's built-in tag reading (no extra deps needed)
# OR for more complete metadata:
sudo apt install libtagc0-dev  # TagLib C bindings
```

**Recommended approach:** Use GStreamer's discoverer API (already have gstreamer dep)

### Implementation

**1. Add to nemo-window-slot.h struct:**
```c
GtkWidget *preview_audio_box;      /* Container for audio metadata */
GtkWidget *preview_audio_cover;    /* Album art GtkImage */
GtkWidget *preview_audio_title;    /* Title label */
GtkWidget *preview_audio_artist;   /* Artist label */
GtkWidget *preview_audio_album;    /* Album label */
GtkWidget *preview_audio_duration; /* Duration label */
```

**2. Add helper functions in nemo-window-slot.c:**
```c
#ifdef HAVE_GSTREAMER
#include <gst/pbutils/pbutils.h>

static gboolean
is_audio_mime_type (const char *mime_type)
{
    if (mime_type == NULL)
        return FALSE;
    return g_str_has_prefix (mime_type, "audio/");
}

static void
show_audio_preview (NemoWindowSlot *slot)
{
    gtk_widget_hide (slot->preview_image_scroll);
    gtk_widget_hide (slot->preview_text_scroll);
    gtk_widget_hide (slot->preview_video_box);
    gtk_widget_hide (slot->preview_video_error_box);
    gtk_widget_show (slot->preview_audio_box);
}

static gchar *
format_duration (gint64 duration_ns)
{
    gint64 seconds = duration_ns / GST_SECOND;
    gint64 minutes = seconds / 60;
    seconds = seconds % 60;

    if (minutes >= 60) {
        gint64 hours = minutes / 60;
        minutes = minutes % 60;
        return g_strdup_printf ("%ld:%02ld:%02ld", hours, minutes, seconds);
    }
    return g_strdup_printf ("%ld:%02ld", minutes, seconds);
}

static void
on_audio_discovered (GstDiscoverer *discoverer,
                     GstDiscovererInfo *info,
                     GError *error,
                     NemoWindowSlot *slot)
{
    const GstTagList *tags;
    gchar *title = NULL, *artist = NULL, *album = NULL;
    gchar *duration_str;
    GstSample *sample = NULL;
    GstDiscovererResult result;

    result = gst_discoverer_info_get_result (info);
    if (result != GST_DISCOVERER_OK) {
        gtk_label_set_text (GTK_LABEL (slot->preview_audio_title), _("Unknown"));
        return;
    }

    /* Get duration */
    duration_str = format_duration (gst_discoverer_info_get_duration (info));
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_duration), duration_str);
    g_free (duration_str);

    /* Get tags */
    tags = gst_discoverer_info_get_tags (info);
    if (tags != NULL) {
        gst_tag_list_get_string (tags, GST_TAG_TITLE, &title);
        gst_tag_list_get_string (tags, GST_TAG_ARTIST, &artist);
        gst_tag_list_get_string (tags, GST_TAG_ALBUM, &album);
        gst_tag_list_get_sample (tags, GST_TAG_IMAGE, &sample);
    }

    gtk_label_set_text (GTK_LABEL (slot->preview_audio_title),
                        title ? title : _("Unknown Title"));
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_artist),
                        artist ? artist : _("Unknown Artist"));
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_album),
                        album ? album : "");

    /* Extract cover art if available */
    if (sample != NULL) {
        GstBuffer *buffer = gst_sample_get_buffer (sample);
        GstMapInfo map;

        if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
            GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
            GdkPixbuf *pixbuf;

            gdk_pixbuf_loader_write (loader, map.data, map.size, NULL);
            gdk_pixbuf_loader_close (loader, NULL);
            pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

            if (pixbuf != NULL) {
                GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf,
                    150, 150, GDK_INTERP_BILINEAR);
                gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_audio_cover),
                                           scaled);
                g_object_unref (scaled);
            }

            gst_buffer_unmap (buffer, &map);
            g_object_unref (loader);
        }
        gst_sample_unref (sample);
    } else {
        /* No cover art - show music icon */
        gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_audio_cover),
                                      "audio-x-generic", GTK_ICON_SIZE_DIALOG);
    }

    g_free (title);
    g_free (artist);
    g_free (album);
}

static void
load_audio_metadata (NemoWindowSlot *slot, const char *uri)
{
    GstDiscoverer *discoverer;
    GError *error = NULL;

    /* Reset display */
    gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_audio_cover),
                                  "audio-x-generic", GTK_ICON_SIZE_DIALOG);
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_title), _("Loading..."));
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_artist), "");
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_album), "");
    gtk_label_set_text (GTK_LABEL (slot->preview_audio_duration), "");

    discoverer = gst_discoverer_new (5 * GST_SECOND, &error);
    if (discoverer == NULL) {
        g_warning ("Could not create discoverer: %s", error->message);
        g_error_free (error);
        return;
    }

    g_signal_connect (discoverer, "discovered",
                      G_CALLBACK (on_audio_discovered), slot);

    gst_discoverer_start (discoverer);
    gst_discoverer_discover_uri_async (discoverer, uri);

    /* Note: discoverer needs to be managed (stored in slot, unreffed on cleanup) */
}
#endif
```

**3. Add widgets in create_preview_pane():**
```c
#ifdef HAVE_GSTREAMER
/* Audio metadata display */
{
    GtkWidget *audio_box, *cover, *info_box;
    GtkWidget *title_label, *artist_label, *album_label, *duration_label;
    PangoAttrList *attrs;

    audio_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_valign (audio_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign (audio_box, GTK_ALIGN_CENTER);
    slot->preview_audio_box = audio_box;

    /* Cover art */
    cover = gtk_image_new_from_icon_name ("audio-x-generic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size (GTK_IMAGE (cover), 150);
    slot->preview_audio_cover = cover;
    gtk_box_pack_start (GTK_BOX (audio_box), cover, FALSE, FALSE, 0);

    /* Info container */
    info_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign (info_box, GTK_ALIGN_CENTER);

    /* Title (bold) */
    title_label = gtk_label_new ("");
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (title_label), attrs);
    pango_attr_list_unref (attrs);
    gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (GTK_LABEL (title_label), 25);
    slot->preview_audio_title = title_label;
    gtk_box_pack_start (GTK_BOX (info_box), title_label, FALSE, FALSE, 0);

    /* Artist */
    artist_label = gtk_label_new ("");
    gtk_label_set_ellipsize (GTK_LABEL (artist_label), PANGO_ELLIPSIZE_END);
    slot->preview_audio_artist = artist_label;
    gtk_box_pack_start (GTK_BOX (info_box), artist_label, FALSE, FALSE, 0);

    /* Album */
    album_label = gtk_label_new ("");
    gtk_label_set_ellipsize (GTK_LABEL (album_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_opacity (album_label, 0.7);
    slot->preview_audio_album = album_label;
    gtk_box_pack_start (GTK_BOX (info_box), album_label, FALSE, FALSE, 0);

    /* Duration */
    duration_label = gtk_label_new ("");
    slot->preview_audio_duration = duration_label;
    gtk_box_pack_start (GTK_BOX (info_box), duration_label, FALSE, FALSE, 0);

    gtk_box_pack_start (GTK_BOX (audio_box), info_box, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), audio_box, TRUE, TRUE, 0);
}
#endif
```

### Testing
- Select MP3/FLAC/OGG → shows title, artist, album, duration
- Files with embedded cover art → displays cover image
- Files without metadata → shows "Unknown" placeholders
- Non-audio files → doesn't trigger audio preview

---

## Enhancement: Animated GIF Support

### Overview
Display animated GIFs with animation playing in the preview pane.

### Dependencies
**No additional dependencies** - GdkPixbuf already supports GIF animations.

### Implementation

**1. Add to nemo-window-slot.h struct:**
```c
GdkPixbufAnimation *preview_animation;  /* For animated images */
GdkPixbufAnimationIter *preview_anim_iter;
guint preview_anim_timeout_id;          /* Timer for animation frames */
```

**2. Add helper functions in nemo-window-slot.c:**
```c
static void
stop_gif_animation (NemoWindowSlot *slot)
{
    if (slot->preview_anim_timeout_id > 0) {
        g_source_remove (slot->preview_anim_timeout_id);
        slot->preview_anim_timeout_id = 0;
    }
    if (slot->preview_anim_iter != NULL) {
        g_object_unref (slot->preview_anim_iter);
        slot->preview_anim_iter = NULL;
    }
    if (slot->preview_animation != NULL) {
        g_object_unref (slot->preview_animation);
        slot->preview_animation = NULL;
    }
}

static gboolean
update_gif_frame (gpointer user_data)
{
    NemoWindowSlot *slot = NEMO_WINDOW_SLOT (user_data);
    GdkPixbuf *pixbuf;
    int delay_ms;

    if (slot->preview_anim_iter == NULL) {
        slot->preview_anim_timeout_id = 0;
        return FALSE;
    }

    /* Advance to next frame */
    gdk_pixbuf_animation_iter_advance (slot->preview_anim_iter, NULL);

    pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (slot->preview_anim_iter);
    if (pixbuf != NULL) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_image), pixbuf);
    }

    /* Get delay for next frame */
    delay_ms = gdk_pixbuf_animation_iter_get_delay_time (slot->preview_anim_iter);
    if (delay_ms < 0) {
        /* Static image or end of animation */
        slot->preview_anim_timeout_id = 0;
        return FALSE;
    }

    /* Schedule next frame update */
    slot->preview_anim_timeout_id = g_timeout_add (delay_ms, update_gif_frame, slot);
    return FALSE;  /* Remove this timeout, new one scheduled */
}

static gboolean
is_animated_image (const char *path)
{
    GdkPixbufAnimation *anim;
    gboolean is_animated = FALSE;

    anim = gdk_pixbuf_animation_new_from_file (path, NULL);
    if (anim != NULL) {
        is_animated = !gdk_pixbuf_animation_is_static_image (anim);
        g_object_unref (anim);
    }
    return is_animated;
}

static void
load_animated_image (NemoWindowSlot *slot, const char *path, int max_width)
{
    GdkPixbufAnimation *anim;
    GdkPixbuf *pixbuf, *scaled;
    int orig_width, orig_height, new_width, new_height;
    double scale;

    stop_gif_animation (slot);

    anim = gdk_pixbuf_animation_new_from_file (path, NULL);
    if (anim == NULL) {
        gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_image),
                                      "image-missing", GTK_ICON_SIZE_DIALOG);
        return;
    }

    if (gdk_pixbuf_animation_is_static_image (anim)) {
        /* Not animated, just show static image */
        pixbuf = gdk_pixbuf_animation_get_static_image (anim);
        orig_width = gdk_pixbuf_get_width (pixbuf);
        orig_height = gdk_pixbuf_get_height (pixbuf);

        if (orig_width > max_width) {
            scale = (double)max_width / orig_width;
            new_width = max_width;
            new_height = (int)(orig_height * scale);
            scaled = gdk_pixbuf_scale_simple (pixbuf, new_width, new_height,
                                              GDK_INTERP_BILINEAR);
            gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_image), scaled);
            g_object_unref (scaled);
        } else {
            gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_image), pixbuf);
        }
        g_object_unref (anim);
        return;
    }

    /* Animated image */
    slot->preview_animation = anim;
    slot->preview_anim_iter = gdk_pixbuf_animation_get_iter (anim, NULL);

    /* Show first frame */
    pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (slot->preview_anim_iter);
    if (pixbuf != NULL) {
        gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_image), pixbuf);
    }

    /* Start animation timer */
    int delay_ms = gdk_pixbuf_animation_iter_get_delay_time (slot->preview_anim_iter);
    if (delay_ms > 0) {
        slot->preview_anim_timeout_id = g_timeout_add (delay_ms, update_gif_frame, slot);
    }
}
```

**3. Modify image loading in update_preview_for_file():**
```c
/* Check if it's an image */
if (mime_type != NULL && g_str_has_prefix (mime_type, "image/")) {
    show_image_preview (slot);

    location = nemo_file_get_location (file);
    path = g_file_get_path (location);

    if (path != NULL) {
        preview_width = gtk_widget_get_allocated_width (slot->preview_pane);
        if (preview_width < PREVIEW_PANE_MIN_WIDTH) {
            preview_width = PREVIEW_PANE_DEFAULT_WIDTH;
        }

        /* Check for animated GIF */
        if (g_str_has_suffix (path, ".gif") || g_str_has_suffix (path, ".GIF")) {
            load_animated_image (slot, path, preview_width - 20);
        } else {
            /* Regular static image loading */
            stop_gif_animation (slot);  /* Stop any running animation */
            pixbuf = gdk_pixbuf_new_from_file_at_scale (path,
                preview_width - 20, -1, TRUE, &error);
            /* ... rest of existing code ... */
        }
        g_free (path);
    }
    g_object_unref (location);
}
```

**4. Add cleanup in show_*_preview functions and dispose:**
```c
/* In show_text_preview, show_video_preview, etc: */
stop_gif_animation (slot);

/* In nemo_window_slot_dispose: */
stop_gif_animation (slot);
```

### Testing
- Select animated GIF → animation plays
- Select static GIF → shows as static image
- Select other image type → normal image preview
- Switch selection → previous animation stops, new one starts
- Close preview pane → animation stops cleanly

### Performance Considerations
- GIF animation uses a timer per frame (typically 10-100ms intervals)
- Only one animation runs at a time
- Animation stops when selection changes or pane closes
- Large GIFs may use significant memory - consider adding size limits
