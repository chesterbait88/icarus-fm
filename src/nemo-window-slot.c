/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-window-slot.c: Nemo window slot

   Copyright (C) 2008 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "config.h"

#include "nemo-window-slot.h"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#endif

#ifdef HAVE_POPPLER
#include <poppler.h>
#endif

#include "nemo-actions.h"
#include "nemo-desktop-window.h"
#include "nemo-floating-bar.h"
#include "nemo-window-private.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-types.h"
#include "nemo-window-slot-dnd.h"

#include <glib/gi18n.h>

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

#include <eel/eel-string.h>

G_DEFINE_TYPE (NemoWindowSlot, nemo_window_slot, GTK_TYPE_BOX);

#define PREVIEW_PANE_MIN_WIDTH 200
#define PREVIEW_PANE_DEFAULT_WIDTH 300
#define PREVIEW_TEXT_MAX_SIZE (1024 * 1024)  /* 1MB max for text preview */

enum {
	ACTIVE,
	INACTIVE,
	CHANGED_PANE,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
sync_search_directory (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoQuery *query;

	g_assert (NEMO_IS_FILE (slot->viewed_file));

	directory = nemo_directory_get_for_file (slot->viewed_file);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	query = nemo_query_editor_get_query (slot->query_editor);

    if (query) {
        nemo_query_set_show_hidden (query,
                                    g_settings_get_boolean (nemo_preferences,
                                                            NEMO_PREFERENCES_SHOW_HIDDEN_FILES));
    }

	nemo_search_directory_set_query (NEMO_SEARCH_DIRECTORY (directory),
					     query);

    g_clear_object (&query);

	nemo_window_slot_force_reload (slot);

	nemo_directory_unref (directory);
}

static void
sync_search_location_cb (NemoWindow *window,
			 GError *error,
			 gpointer user_data)
{
	NemoWindowSlot *slot = user_data;

	sync_search_directory (slot);
}

static void
create_new_search (NemoWindowSlot *slot)
{
	char *uri;
	NemoDirectory *directory;
	GFile *location;

	uri = nemo_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);

	directory = nemo_directory_get (location);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	nemo_window_slot_open_location_full (slot, location, NEMO_WINDOW_OPEN_FLAG_SEARCH, NULL, sync_search_location_cb, slot);

	nemo_directory_unref (directory);
	g_object_unref (location);
	g_free (uri);
}

static void
query_editor_cancel_callback (NemoQueryEditor *editor,
			      NemoWindowSlot *slot)
{
	GtkAction *search;

	search = gtk_action_group_get_action (slot->pane->toolbar_action_group,
					      NEMO_ACTION_SEARCH);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (search), FALSE);
}

static void
query_editor_changed_callback (NemoQueryEditor *editor,
			       NemoQuery *query,
			       gboolean reload,
			       NemoWindowSlot *slot)
{
	NemoDirectory *directory;

	g_assert (NEMO_IS_FILE (slot->viewed_file));

    gtk_widget_hide (slot->no_search_results_box);
	directory = nemo_directory_get_for_file (slot->viewed_file);
	if (!NEMO_IS_SEARCH_DIRECTORY (directory)) {
		/* this is the first change from the query editor. we
		   ask for a location change to the search directory,
		   indicate the directory needs to be sync'd with the
		   current query. */
		create_new_search (slot);
		/* Focus is now on the new slot, move it back to query_editor */
		gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
	} else {
		sync_search_directory (slot);
	}

	nemo_directory_unref (directory);
}

static void
update_query_editor (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoSearchDirectory *search_directory;

	directory = nemo_directory_get (slot->location);

	if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
		NemoQuery *query;
		search_directory = NEMO_SEARCH_DIRECTORY (directory);
		query = nemo_search_directory_get_query (search_directory);
		if (query != NULL) {
			nemo_query_editor_set_query (slot->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nemo_query_editor_set_location (slot->query_editor, slot->location);
	}

	nemo_directory_unref (directory);
}

static void
ensure_query_editor (NemoWindowSlot *slot)
{
	g_assert (slot->query_editor != NULL);

	update_query_editor (slot);

    nemo_query_editor_set_active (NEMO_QUERY_EDITOR (slot->query_editor),
                                  nemo_window_slot_get_location_uri (slot),
                                  TRUE);

	gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
}

void
nemo_window_slot_set_query_editor_visible (NemoWindowSlot *slot,
					       gboolean            visible)
{
    gtk_widget_hide (slot->no_search_results_box);

	if (visible) {
		ensure_query_editor (slot);

		if (slot->qe_changed_id == 0)
			slot->qe_changed_id = g_signal_connect (slot->query_editor, "changed",
								G_CALLBACK (query_editor_changed_callback), slot);
		if (slot->qe_cancel_id == 0)
			slot->qe_cancel_id = g_signal_connect (slot->query_editor, "cancel",
							       G_CALLBACK (query_editor_cancel_callback), slot);

	} else {
        nemo_query_editor_set_active (NEMO_QUERY_EDITOR (slot->query_editor), NULL, FALSE);

        if (slot->qe_changed_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_changed_id);
            slot->qe_changed_id = 0;
        }

        if (slot->qe_cancel_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_cancel_id);
            slot->qe_cancel_id = 0;
        }

        nemo_query_editor_set_query (slot->query_editor, NULL);
	}
}

static void
real_active (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoWindowPane *pane;
	int page_num;

	window = nemo_window_slot_get_window (slot);
	pane = slot->pane;
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (pane->notebook),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (pane->notebook), page_num);

	/* sync window to new slot */
	nemo_window_push_status (window, slot->status_text);
	nemo_window_sync_allow_stop (window, slot);
	nemo_window_sync_title (window, slot);
	nemo_window_sync_zoom_widgets (window);
    nemo_window_sync_bookmark_action (window);
	nemo_window_pane_sync_location_widgets (slot->pane);
	nemo_window_pane_sync_search_widgets (slot->pane);
	nemo_window_sync_thumbnail_action(window);

	if (slot->viewed_file != NULL) {
		nemo_window_sync_view_type (window);
		nemo_window_load_extension_menus (window);
	}
}

static void
real_inactive (NemoWindowSlot *slot)
{
	NemoWindow *window;

	window = nemo_window_slot_get_window (slot);
	g_assert (slot == nemo_window_get_active_slot (window));
}

static void
floating_bar_action_cb (NemoFloatingBar *floating_bar,
			gint action,
			NemoWindowSlot *slot)
{
	if (action == NEMO_FLOATING_BAR_ACTION_ID_STOP) {
		nemo_window_slot_stop_loading (slot);
	}
}

static GtkWidget *
create_nsr_box (void)
{
    GtkWidget *box;
    GtkWidget *widget;
    PangoAttrList *attrs;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

    widget = gtk_image_new_from_icon_name ("xsi-search-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    widget = gtk_label_new (_("No files found"));
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_size_new (20 * PANGO_SCALE));
    gtk_label_set_attributes (GTK_LABEL (widget), attrs);
    pango_attr_list_unref (attrs);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

    gtk_widget_show_all (box);
    gtk_widget_set_no_show_all (box, TRUE);
    gtk_widget_hide (box);
    return box;
}

#ifdef HAVE_GSTREAMER
/* Forward declarations for video preview functions */
static void stop_video_pipeline (NemoWindowSlot *slot);
static void on_timeline_value_changed (GtkRange *range, NemoWindowSlot *slot);
static void show_video_fallback_icon (NemoWindowSlot *slot);
static gboolean start_video_preview (NemoWindowSlot *slot, const char *uri);
#endif

static gboolean
is_text_mime_type (const char *mime_type)
{
	if (mime_type == NULL) {
		return FALSE;
	}

	/* Common text mime types */
	if (g_str_has_prefix (mime_type, "text/")) {
		return TRUE;
	}

	/* Application types that are actually text */
	if (g_strcmp0 (mime_type, "application/json") == 0 ||
	    g_strcmp0 (mime_type, "application/xml") == 0 ||
	    g_strcmp0 (mime_type, "application/javascript") == 0 ||
	    g_strcmp0 (mime_type, "application/x-shellscript") == 0 ||
	    g_strcmp0 (mime_type, "application/x-perl") == 0 ||
	    g_strcmp0 (mime_type, "application/x-python") == 0 ||
	    g_strcmp0 (mime_type, "application/x-ruby") == 0 ||
	    g_strcmp0 (mime_type, "application/x-php") == 0 ||
	    g_strcmp0 (mime_type, "application/x-yaml") == 0 ||
	    g_strcmp0 (mime_type, "application/toml") == 0 ||
	    g_strcmp0 (mime_type, "application/sql") == 0) {
		return TRUE;
	}

	return FALSE;
}

#ifdef HAVE_GSTREAMER
static void stop_audio_pipeline (NemoWindowSlot *slot);
#endif

static void
show_image_preview (NemoWindowSlot *slot)
{
	gtk_widget_show (slot->preview_image_scroll);
	gtk_widget_hide (slot->preview_text_scroll);
#ifdef HAVE_GSTREAMER
	if (slot->preview_video_box != NULL)
		gtk_widget_hide (slot->preview_video_box);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_hide (slot->preview_video_error_box);
	if (slot->preview_audio_box != NULL)
		gtk_widget_hide (slot->preview_audio_box);
	stop_video_pipeline (slot);
	stop_audio_pipeline (slot);
#endif
#ifdef HAVE_POPPLER
	if (slot->preview_pdf_scroll != NULL)
		gtk_widget_hide (slot->preview_pdf_scroll);
#endif
}

static void stop_gif_animation (NemoWindowSlot *slot);

static void
show_text_preview (NemoWindowSlot *slot)
{
	gtk_widget_hide (slot->preview_image_scroll);
	gtk_widget_show (slot->preview_text_scroll);
	stop_gif_animation (slot);
#ifdef HAVE_GSTREAMER
	if (slot->preview_video_box != NULL)
		gtk_widget_hide (slot->preview_video_box);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_hide (slot->preview_video_error_box);
	if (slot->preview_audio_box != NULL)
		gtk_widget_hide (slot->preview_audio_box);
	stop_video_pipeline (slot);
	stop_audio_pipeline (slot);
#endif
#ifdef HAVE_POPPLER
	if (slot->preview_pdf_scroll != NULL)
		gtk_widget_hide (slot->preview_pdf_scroll);
#endif
}

#ifdef HAVE_GSTREAMER
static void
show_video_preview (NemoWindowSlot *slot)
{
	if (slot->preview_image_scroll != NULL)
		gtk_widget_hide (slot->preview_image_scroll);
	if (slot->preview_text_scroll != NULL)
		gtk_widget_hide (slot->preview_text_scroll);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_hide (slot->preview_video_error_box);
	if (slot->preview_audio_box != NULL)
		gtk_widget_hide (slot->preview_audio_box);
	if (slot->preview_video_box != NULL)
		gtk_widget_show (slot->preview_video_box);
	stop_gif_animation (slot);
	stop_audio_pipeline (slot);
#ifdef HAVE_POPPLER
	if (slot->preview_pdf_scroll != NULL)
		gtk_widget_hide (slot->preview_pdf_scroll);
#endif
}

/* Show a fallback message when video preview fails */
static void
show_video_fallback_icon (NemoWindowSlot *slot)
{
	/* Hide other preview widgets and show error box */
	if (slot->preview_video_box != NULL)
		gtk_widget_hide (slot->preview_video_box);
	if (slot->preview_text_scroll != NULL)
		gtk_widget_hide (slot->preview_text_scroll);
	if (slot->preview_image_scroll != NULL)
		gtk_widget_hide (slot->preview_image_scroll);
	if (slot->preview_audio_box != NULL)
		gtk_widget_hide (slot->preview_audio_box);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_show (slot->preview_video_error_box);
#ifdef HAVE_POPPLER
	if (slot->preview_pdf_scroll != NULL)
		gtk_widget_hide (slot->preview_pdf_scroll);
#endif
}

static void
stop_video_pipeline (NemoWindowSlot *slot)
{
	if (slot->video_update_id > 0) {
		g_source_remove (slot->video_update_id);
		slot->video_update_id = 0;
	}

	if (slot->video_pipeline != NULL) {
		gst_element_set_state (GST_ELEMENT (slot->video_pipeline), GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (slot->video_pipeline));
		slot->video_pipeline = NULL;
	}

	slot->video_is_playing = FALSE;

	/* Reset button to play icon */
	if (slot->preview_play_button != NULL) {
		GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_play_button));
		if (image != NULL)
			gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
	}
}

static gboolean
update_video_timeline (gpointer user_data)
{
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (user_data);
	gint64 position, duration;

	if (slot == NULL || slot->video_pipeline == NULL) {
		slot->video_update_id = 0;
		return FALSE;
	}

	if (slot->preview_timeline == NULL) {
		return TRUE;  /* Keep timer running, widget might not be ready yet */
	}

	if (gst_element_query_position (GST_ELEMENT (slot->video_pipeline), GST_FORMAT_TIME, &position) &&
	    gst_element_query_duration (GST_ELEMENT (slot->video_pipeline), GST_FORMAT_TIME, &duration)) {
		if (duration > 0) {
			gdouble fraction = (gdouble) position / (gdouble) duration;
			g_signal_handlers_block_by_func (slot->preview_timeline,
							 G_CALLBACK (on_timeline_value_changed), slot);
			gtk_range_set_value (GTK_RANGE (slot->preview_timeline), fraction * 100.0);
			g_signal_handlers_unblock_by_func (slot->preview_timeline,
							   G_CALLBACK (on_timeline_value_changed), slot);
		}
	}

	return TRUE;
}

static void
on_play_button_clicked (GtkButton *button, NemoWindowSlot *slot)
{
	GtkWidget *image;

	if (slot == NULL || slot->video_pipeline == NULL) {
		return;
	}

	image = gtk_button_get_image (GTK_BUTTON (button));
	if (image == NULL) {
		return;
	}

	if (slot->video_is_playing) {
		gst_element_set_state (GST_ELEMENT (slot->video_pipeline), GST_STATE_PAUSED);
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		slot->video_is_playing = FALSE;
	} else {
		gst_element_set_state (GST_ELEMENT (slot->video_pipeline), GST_STATE_PLAYING);
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);
		slot->video_is_playing = TRUE;
	}
}

static void
on_timeline_value_changed (GtkRange *range, NemoWindowSlot *slot)
{
	gdouble value;
	gint64 duration, position;

	if (slot == NULL || slot->video_pipeline == NULL) {
		return;
	}

	value = gtk_range_get_value (range);

	if (gst_element_query_duration (GST_ELEMENT (slot->video_pipeline), GST_FORMAT_TIME, &duration)) {
		position = (gint64) ((value / 100.0) * duration);
		gst_element_seek_simple (GST_ELEMENT (slot->video_pipeline),
					 GST_FORMAT_TIME,
					 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
					 position);
	}
}

static void
on_video_bus_message (GstBus *bus, GstMessage *message, NemoWindowSlot *slot)
{
	if (slot == NULL)
		return;

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_EOS:
		/* End of stream - reset to beginning and pause */
		if (slot->video_pipeline != NULL) {
			gst_element_seek_simple (GST_ELEMENT (slot->video_pipeline),
						 GST_FORMAT_TIME,
						 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
						 0);
			gst_element_set_state (GST_ELEMENT (slot->video_pipeline), GST_STATE_PAUSED);
		}
		slot->video_is_playing = FALSE;
		if (slot->preview_play_button != NULL) {
			GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_play_button));
			if (image != NULL)
				gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		}
		if (slot->preview_timeline != NULL)
			gtk_range_set_value (GTK_RANGE (slot->preview_timeline), 0.0);
		break;
	case GST_MESSAGE_ERROR: {
		GError *error = NULL;
		gst_message_parse_error (message, &error, NULL);
		g_warning ("Video playback error: %s", error->message);
		g_error_free (error);
		stop_video_pipeline (slot);
		/* Show fallback UI when playback fails */
		show_video_fallback_icon (slot);
		break;
	}
	default:
		break;
	}
}

static gboolean
is_video_mime_type (const char *mime_type)
{
	if (mime_type == NULL) {
		return FALSE;
	}

	return g_str_has_prefix (mime_type, "video/");
}

static gboolean
start_video_preview (NemoWindowSlot *slot, const char *uri)
{
	GstElement *pipeline;
	GstElement *playbin;
	GstElement *video_sink;
	GstBus *bus;

	/* Stop any existing pipeline */
	stop_video_pipeline (slot);

	/* Create playbin pipeline */
	playbin = gst_element_factory_make ("playbin", "playbin");
	if (playbin == NULL) {
		g_warning ("Could not create playbin element - GStreamer may not be properly installed");
		return FALSE;
	}

	/* Create GTK video sink */
	video_sink = gst_element_factory_make ("gtksink", "gtksink");
	if (video_sink == NULL) {
		/* Try gtkglsink as fallback */
		video_sink = gst_element_factory_make ("gtkglsink", "gtkglsink");
	}

	if (video_sink == NULL) {
		g_warning ("Could not create GTK video sink - please install gstreamer1.0-gtk3 package");
		gst_object_unref (playbin);
		return FALSE;
	}

	/* Get the widget from the sink and add it to our container */
	GtkWidget *video_widget = NULL;
	g_object_get (video_sink, "widget", &video_widget, NULL);

	if (video_widget != NULL) {
		/* Remove old video widget if any */
		GList *children = gtk_container_get_children (GTK_CONTAINER (slot->preview_video_widget));
		for (GList *l = children; l != NULL; l = l->next) {
			gtk_container_remove (GTK_CONTAINER (slot->preview_video_widget), GTK_WIDGET (l->data));
		}
		g_list_free (children);

		gtk_container_add (GTK_CONTAINER (slot->preview_video_widget), video_widget);
		gtk_widget_show (video_widget);
		g_object_unref (video_widget);
	}

	/* Set up playbin */
	g_object_set (playbin, "uri", uri, "video-sink", video_sink, NULL);

	pipeline = playbin;
	slot->video_pipeline = pipeline;

	/* Set up bus watch */
	bus = gst_element_get_bus (pipeline);
	gst_bus_add_signal_watch (bus);
	g_signal_connect (bus, "message", G_CALLBACK (on_video_bus_message), slot);
	gst_object_unref (bus);

	/* Start paused so we can see first frame */
	gst_element_set_state (pipeline, GST_STATE_PAUSED);
	slot->video_is_playing = FALSE;

	/* Reset timeline */
	if (slot->preview_timeline != NULL)
		gtk_range_set_value (GTK_RANGE (slot->preview_timeline), 0.0);

	/* Reset play button */
	if (slot->preview_play_button != NULL) {
		GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_play_button));
		if (image != NULL)
			gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
	}

	/* Start timeline update timer */
	if (slot->video_update_id > 0) {
		g_source_remove (slot->video_update_id);
	}
	slot->video_update_id = g_timeout_add (250, update_video_timeline, slot);

	return TRUE;
}
#endif /* HAVE_GSTREAMER */

/* Animated GIF support functions */
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

static void
load_animated_image (NemoWindowSlot *slot, const char *path, int max_width)
{
	GdkPixbufAnimation *anim;
	GdkPixbuf *pixbuf, *scaled;
	int orig_width, orig_height, new_width, new_height;
	double scale;
	int delay_ms;

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
	delay_ms = gdk_pixbuf_animation_iter_get_delay_time (slot->preview_anim_iter);
	if (delay_ms > 0) {
		slot->preview_anim_timeout_id = g_timeout_add (delay_ms, update_gif_frame, slot);
	}
}

#ifdef HAVE_POPPLER
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
	stop_gif_animation (slot);
#ifdef HAVE_GSTREAMER
	if (slot->preview_video_box != NULL)
		gtk_widget_hide (slot->preview_video_box);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_hide (slot->preview_video_error_box);
	if (slot->preview_audio_box != NULL)
		gtk_widget_hide (slot->preview_audio_box);
	stop_video_pipeline (slot);
	stop_audio_pipeline (slot);
#endif
	if (slot->preview_pdf_scroll != NULL)
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
#endif /* HAVE_POPPLER */

#ifdef HAVE_GSTREAMER
/* Audio preview functions */
static void on_audio_timeline_value_changed (GtkRange *range, NemoWindowSlot *slot);

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
	stop_gif_animation (slot);
	if (slot->preview_video_box != NULL)
		gtk_widget_hide (slot->preview_video_box);
	if (slot->preview_video_error_box != NULL)
		gtk_widget_hide (slot->preview_video_error_box);
	stop_video_pipeline (slot);
#ifdef HAVE_POPPLER
	if (slot->preview_pdf_scroll != NULL)
		gtk_widget_hide (slot->preview_pdf_scroll);
#endif
	if (slot->preview_audio_box != NULL)
		gtk_widget_show (slot->preview_audio_box);
}

static void
stop_audio_pipeline (NemoWindowSlot *slot)
{
	if (slot->audio_update_id > 0) {
		g_source_remove (slot->audio_update_id);
		slot->audio_update_id = 0;
	}

	if (slot->audio_pipeline != NULL) {
		gst_element_set_state (GST_ELEMENT (slot->audio_pipeline), GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (slot->audio_pipeline));
		slot->audio_pipeline = NULL;
	}

	slot->audio_is_playing = FALSE;

	/* Reset button to play icon */
	if (slot->preview_audio_play_btn != NULL) {
		GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_audio_play_btn));
		if (image != NULL)
			gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
	}
}

static gboolean
update_audio_timeline (gpointer user_data)
{
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (user_data);
	gint64 position, duration;

	if (slot == NULL || slot->audio_pipeline == NULL) {
		slot->audio_update_id = 0;
		return FALSE;
	}

	if (slot->preview_audio_timeline == NULL) {
		return TRUE;
	}

	if (gst_element_query_position (GST_ELEMENT (slot->audio_pipeline), GST_FORMAT_TIME, &position) &&
	    gst_element_query_duration (GST_ELEMENT (slot->audio_pipeline), GST_FORMAT_TIME, &duration)) {
		if (duration > 0) {
			gdouble fraction = (gdouble) position / (gdouble) duration;
			g_signal_handlers_block_by_func (slot->preview_audio_timeline,
							 G_CALLBACK (on_audio_timeline_value_changed), slot);
			gtk_range_set_value (GTK_RANGE (slot->preview_audio_timeline), fraction * 100.0);
			g_signal_handlers_unblock_by_func (slot->preview_audio_timeline,
							   G_CALLBACK (on_audio_timeline_value_changed), slot);
		}
	}

	return TRUE;
}

static void
on_audio_play_button_clicked (GtkButton *button, NemoWindowSlot *slot)
{
	GtkWidget *image;

	if (slot == NULL || slot->audio_pipeline == NULL) {
		return;
	}

	image = gtk_button_get_image (GTK_BUTTON (button));
	if (image == NULL) {
		return;
	}

	if (slot->audio_is_playing) {
		gst_element_set_state (GST_ELEMENT (slot->audio_pipeline), GST_STATE_PAUSED);
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		slot->audio_is_playing = FALSE;
	} else {
		gst_element_set_state (GST_ELEMENT (slot->audio_pipeline), GST_STATE_PLAYING);
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);
		slot->audio_is_playing = TRUE;
	}
}

static void
on_audio_timeline_value_changed (GtkRange *range, NemoWindowSlot *slot)
{
	gdouble value;
	gint64 duration, position;

	if (slot == NULL || slot->audio_pipeline == NULL) {
		return;
	}

	value = gtk_range_get_value (range);

	if (gst_element_query_duration (GST_ELEMENT (slot->audio_pipeline), GST_FORMAT_TIME, &duration)) {
		position = (gint64) ((value / 100.0) * duration);
		gst_element_seek_simple (GST_ELEMENT (slot->audio_pipeline),
					 GST_FORMAT_TIME,
					 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
					 position);
	}
}

static void
on_audio_bus_message (GstBus *bus, GstMessage *message, NemoWindowSlot *slot)
{
	if (slot == NULL)
		return;

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_EOS:
		/* End of stream - reset to beginning and pause */
		if (slot->audio_pipeline != NULL) {
			gst_element_seek_simple (GST_ELEMENT (slot->audio_pipeline),
						 GST_FORMAT_TIME,
						 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
						 0);
			gst_element_set_state (GST_ELEMENT (slot->audio_pipeline), GST_STATE_PAUSED);
		}
		slot->audio_is_playing = FALSE;
		if (slot->preview_audio_play_btn != NULL) {
			GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_audio_play_btn));
			if (image != NULL)
				gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		}
		if (slot->preview_audio_timeline != NULL)
			gtk_range_set_value (GTK_RANGE (slot->preview_audio_timeline), 0.0);
		break;
	case GST_MESSAGE_ERROR: {
		GError *error = NULL;
		gst_message_parse_error (message, &error, NULL);
		g_warning ("Audio playback error: %s", error->message);
		g_error_free (error);
		stop_audio_pipeline (slot);
		break;
	}
	default:
		break;
	}
}

static gboolean
start_audio_preview (NemoWindowSlot *slot, const char *uri)
{
	GstElement *pipeline;
	GstElement *playbin;
	GstBus *bus;

	/* Stop any existing pipeline */
	stop_audio_pipeline (slot);

	/* Create playbin pipeline */
	playbin = gst_element_factory_make ("playbin", "audio_playbin");
	if (playbin == NULL) {
		g_warning ("Could not create playbin element for audio");
		return FALSE;
	}

	/* Set up playbin - no video sink needed for audio */
	g_object_set (playbin, "uri", uri, NULL);

	pipeline = playbin;
	slot->audio_pipeline = pipeline;

	/* Set up bus watch */
	bus = gst_element_get_bus (pipeline);
	gst_bus_add_signal_watch (bus);
	g_signal_connect (bus, "message", G_CALLBACK (on_audio_bus_message), slot);
	gst_object_unref (bus);

	/* Start paused */
	gst_element_set_state (pipeline, GST_STATE_PAUSED);
	slot->audio_is_playing = FALSE;

	/* Reset timeline */
	if (slot->preview_audio_timeline != NULL)
		gtk_range_set_value (GTK_RANGE (slot->preview_audio_timeline), 0.0);

	/* Reset play button */
	if (slot->preview_audio_play_btn != NULL) {
		GtkWidget *image = gtk_button_get_image (GTK_BUTTON (slot->preview_audio_play_btn));
		if (image != NULL)
			gtk_image_set_from_icon_name (GTK_IMAGE (image), "media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
	}

	/* Start timeline update timer */
	if (slot->audio_update_id > 0) {
		g_source_remove (slot->audio_update_id);
	}
	slot->audio_update_id = g_timeout_add (250, update_audio_timeline, slot);

	return TRUE;
}
#endif /* HAVE_GSTREAMER */

static void
update_preview_for_file (NemoWindowSlot *slot, NemoFile *file)
{
	char *mime_type;
	char *path;
	char *name;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	int preview_width;
	GFile *location;

	if (file == NULL) {
		stop_gif_animation (slot);
		gtk_image_clear (GTK_IMAGE (slot->preview_image));
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (slot->preview_text_view)),
			"", -1);
		gtk_label_set_text (GTK_LABEL (slot->preview_label), _("No file selected"));
		show_image_preview (slot);
		return;
	}

	mime_type = nemo_file_get_mime_type (file);
	name = nemo_file_get_display_name (file);

	/* Update the label with filename */
	gtk_label_set_text (GTK_LABEL (slot->preview_label), name);

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
			if (g_strcmp0 (mime_type, "image/gif") == 0) {
				load_animated_image (slot, path, preview_width - 20);
			} else {
				/* Regular static image loading - stop any running animation first */
				stop_gif_animation (slot);

				/* Load image scaled to fit preview pane width */
				pixbuf = gdk_pixbuf_new_from_file_at_scale (
					path,
					preview_width - 20,  /* Leave some padding */
					-1,                   /* Height: preserve aspect ratio */
					TRUE,                 /* Preserve aspect ratio */
					&error
				);

				if (pixbuf != NULL) {
					gtk_image_set_from_pixbuf (GTK_IMAGE (slot->preview_image), pixbuf);
					g_object_unref (pixbuf);
				} else {
					gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_image),
								     "image-missing",
								     GTK_ICON_SIZE_DIALOG);
					if (error != NULL) {
						g_warning ("Failed to load preview for %s: %s", path, error->message);
						g_error_free (error);
					}
				}
			}

			g_free (path);
		} else {
			/* Remote file or no local path */
			stop_gif_animation (slot);
			gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_image),
						     "image-x-generic",
						     GTK_ICON_SIZE_DIALOG);
		}

		g_object_unref (location);
	}
	/* Check if it's a text file */
	else if (is_text_mime_type (mime_type)) {
		show_text_preview (slot);

		location = nemo_file_get_location (file);
		path = g_file_get_path (location);

		if (path != NULL) {
			char *contents = NULL;
			gsize length = 0;
			GtkTextBuffer *buffer;

			buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (slot->preview_text_view));

			/* Load the file contents */
			if (g_file_get_contents (path, &contents, &length, &error)) {
				/* Limit size for performance */
				if (length > PREVIEW_TEXT_MAX_SIZE) {
					char *truncated = g_strndup (contents, PREVIEW_TEXT_MAX_SIZE);
					char *with_notice = g_strdup_printf ("%s\n\n... [truncated - file too large] ...", truncated);
					g_free (truncated);
					g_free (contents);
					contents = with_notice;
					length = strlen (contents);
				}

				/* Check if valid UTF-8, if not try to convert */
				if (g_utf8_validate (contents, length, NULL)) {
					gtk_text_buffer_set_text (buffer, contents, length);
				} else {
					/* Try to convert from locale */
					char *utf8_contents = g_locale_to_utf8 (contents, length, NULL, NULL, NULL);
					if (utf8_contents != NULL) {
						gtk_text_buffer_set_text (buffer, utf8_contents, -1);
						g_free (utf8_contents);
					} else {
						gtk_text_buffer_set_text (buffer, _("[Binary or non-UTF8 content]"), -1);
					}
				}
				g_free (contents);
			} else {
				char *error_msg = g_strdup_printf (_("Could not load file: %s"),
								  error ? error->message : _("Unknown error"));
				gtk_text_buffer_set_text (buffer, error_msg, -1);
				g_free (error_msg);
				if (error != NULL) {
					g_error_free (error);
				}
			}

			g_free (path);
		} else {
			GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (slot->preview_text_view));
			gtk_text_buffer_set_text (buffer, _("[Remote file - preview not available]"), -1);
		}

		g_object_unref (location);
	}
#ifdef HAVE_GSTREAMER
	/* Check if it's a video file */
	else if (is_video_mime_type (mime_type)) {
		char *uri;
		gboolean success = FALSE;

		location = nemo_file_get_location (file);
		uri = g_file_get_uri (location);

		if (uri != NULL) {
			show_video_preview (slot);
			success = start_video_preview (slot, uri);
			g_free (uri);
		}

		g_object_unref (location);

		/* Show fallback UI if video preview failed to start */
		if (!success) {
			show_video_fallback_icon (slot);
		}
	}
	/* Check if it's an audio file */
	else if (is_audio_mime_type (mime_type)) {
		char *uri;

		location = nemo_file_get_location (file);
		uri = g_file_get_uri (location);

		if (uri != NULL) {
			show_audio_preview (slot);
			start_audio_preview (slot, uri);
			g_free (uri);
		}

		g_object_unref (location);
	}
#endif
#ifdef HAVE_POPPLER
	/* Check if it's a PDF file */
	else if (is_pdf_mime_type (mime_type)) {
		show_pdf_preview (slot);
		location = nemo_file_get_location (file);
		path = g_file_get_path (location);
		if (path != NULL) {
			load_pdf_preview (slot, path);
			g_free (path);
		} else {
			gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_pdf_image),
						      "application-pdf", GTK_ICON_SIZE_DIALOG);
		}
		g_object_unref (location);
	}
#endif
	else {
		/* Not an image, text, or video - show file icon */
		show_image_preview (slot);
		gtk_image_set_from_icon_name (GTK_IMAGE (slot->preview_image),
					     "text-x-generic",
					     GTK_ICON_SIZE_DIALOG);
	}

	g_free (mime_type);
	g_free (name);
}

static void
on_selection_changed (NemoView *view, NemoWindowSlot *slot)
{
	GList *selection;
	NemoFile *file;

	if (!slot->preview_visible) {
		return;
	}

	selection = nemo_view_get_selection (view);

	if (selection != NULL && g_list_length (selection) == 1) {
		file = NEMO_FILE (selection->data);
		update_preview_for_file (slot, file);
	} else if (selection != NULL && g_list_length (selection) > 1) {
		/* Multiple files selected */
		gtk_image_clear (GTK_IMAGE (slot->preview_image));
		char *label = g_strdup_printf (_("%d items selected"), g_list_length (selection));
		gtk_label_set_text (GTK_LABEL (slot->preview_label), label);
		g_free (label);
	} else {
		update_preview_for_file (slot, NULL);
	}

	nemo_file_list_free (selection);
}

#ifdef HAVE_GSTREAMER
static void
on_video_help_link_clicked (GtkButton *button, gpointer user_data)
{
	GError *error = NULL;
	char *doc_path;
	char *doc_uri;

	/* Build path to local documentation file */
	doc_path = g_build_filename (NEMO_DOCDIR, "video-preview-setup.md", NULL);
	doc_uri = g_filename_to_uri (doc_path, NULL, NULL);

	if (doc_uri != NULL) {
		/* Try to open the local help file */
		if (!gtk_show_uri_on_window (NULL, doc_uri, GDK_CURRENT_TIME, &error)) {
			g_warning ("Could not open help file %s: %s", doc_path, error->message);
			g_error_free (error);
		}
		g_free (doc_uri);
	} else {
		g_warning ("Could not create URI for help file: %s", doc_path);
	}

	g_free (doc_path);
}
#endif

static GtkWidget *
create_preview_pane (NemoWindowSlot *slot)
{
	GtkWidget *box;
	GtkWidget *image_scroll;
	GtkWidget *text_scroll;
	GtkWidget *image;
	GtkWidget *text_view;
	GtkWidget *label;
	PangoFontDescription *font_desc;

	/* Main container */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);
	gtk_widget_set_margin_top (box, 6);
	gtk_widget_set_margin_bottom (box, 6);

	/* Scrolled window for the image */
	image_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (image_scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (image_scroll, TRUE);
	gtk_widget_set_hexpand (image_scroll, TRUE);
	slot->preview_image_scroll = image_scroll;

	/* Image widget */
	image = gtk_image_new ();
	gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
	slot->preview_image = image;

	gtk_container_add (GTK_CONTAINER (image_scroll), image);
	gtk_box_pack_start (GTK_BOX (box), image_scroll, TRUE, TRUE, 0);

	/* Scrolled window for text */
	text_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (text_scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (text_scroll, TRUE);
	gtk_widget_set_hexpand (text_scroll, TRUE);
	slot->preview_text_scroll = text_scroll;

	/* Text view widget */
	text_view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 6);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (text_view), 6);
	gtk_text_view_set_top_margin (GTK_TEXT_VIEW (text_view), 6);
	gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (text_view), 6);

	/* Use monospace font for text preview */
	font_desc = pango_font_description_from_string ("Monospace 9");
	gtk_widget_override_font (text_view, font_desc);
	pango_font_description_free (font_desc);

	slot->preview_text_view = text_view;

	gtk_container_add (GTK_CONTAINER (text_scroll), text_view);
	gtk_box_pack_start (GTK_BOX (box), text_scroll, TRUE, TRUE, 0);

#ifdef HAVE_GSTREAMER
	/* Video preview container */
	{
		GtkWidget *video_box;
		GtkWidget *video_area;
		GtkWidget *controls_box;
		GtkWidget *play_button;
		GtkWidget *play_image;
		GtkWidget *timeline;

		video_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		gtk_widget_set_vexpand (video_box, TRUE);
		gtk_widget_set_hexpand (video_box, TRUE);
		slot->preview_video_box = video_box;

		/* Video drawing area container */
		video_area = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_vexpand (video_area, TRUE);
		gtk_widget_set_hexpand (video_area, TRUE);
		slot->preview_video_widget = video_area;

		gtk_box_pack_start (GTK_BOX (video_box), video_area, TRUE, TRUE, 0);

		/* Controls box */
		controls_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_margin_top (controls_box, 6);

		/* Play/Pause button */
		play_button = gtk_button_new ();
		play_image = gtk_image_new_from_icon_name ("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (play_button), play_image);
		gtk_button_set_relief (GTK_BUTTON (play_button), GTK_RELIEF_NONE);
		gtk_widget_set_can_focus (play_button, FALSE);
		slot->preview_play_button = play_button;

		g_signal_connect (play_button, "clicked",
				  G_CALLBACK (on_play_button_clicked), slot);

		gtk_box_pack_start (GTK_BOX (controls_box), play_button, FALSE, FALSE, 0);

		/* Timeline slider */
		timeline = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
		gtk_scale_set_draw_value (GTK_SCALE (timeline), FALSE);
		gtk_widget_set_hexpand (timeline, TRUE);
		slot->preview_timeline = timeline;

		g_signal_connect (timeline, "value-changed",
				  G_CALLBACK (on_timeline_value_changed), slot);

		gtk_box_pack_start (GTK_BOX (controls_box), timeline, TRUE, TRUE, 0);

		gtk_box_pack_start (GTK_BOX (video_box), controls_box, FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (box), video_box, TRUE, TRUE, 0);
	}

	/* Video error/fallback box - shown when video preview fails */
	{
		GtkWidget *error_box;
		GtkWidget *error_icon;
		GtkWidget *error_label;
		GtkWidget *help_button;

		error_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
		gtk_widget_set_vexpand (error_box, TRUE);
		gtk_widget_set_hexpand (error_box, TRUE);
		gtk_widget_set_valign (error_box, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (error_box, GTK_ALIGN_CENTER);
		slot->preview_video_error_box = error_box;

		/* Video icon */
		error_icon = gtk_image_new_from_icon_name ("video-x-generic", GTK_ICON_SIZE_DIALOG);
		gtk_box_pack_start (GTK_BOX (error_box), error_icon, FALSE, FALSE, 0);

		/* Error message */
		error_label = gtk_label_new (_("Video preview unavailable.\nRequired GStreamer plugins not found."));
		gtk_label_set_justify (GTK_LABEL (error_label), GTK_JUSTIFY_CENTER);
		gtk_label_set_line_wrap (GTK_LABEL (error_label), TRUE);
		gtk_box_pack_start (GTK_BOX (error_box), error_label, FALSE, FALSE, 0);

		/* Help link button */
		help_button = gtk_link_button_new_with_label ("", _("Click here for setup instructions"));
		g_signal_connect (help_button, "clicked",
				  G_CALLBACK (on_video_help_link_clicked), NULL);
		gtk_box_pack_start (GTK_BOX (error_box), help_button, FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (box), error_box, TRUE, TRUE, 0);
	}

	/* Audio preview container */
	{
		GtkWidget *audio_box;
		GtkWidget *audio_icon;
		GtkWidget *controls_box;
		GtkWidget *play_button;
		GtkWidget *play_image;
		GtkWidget *timeline;

		audio_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
		gtk_widget_set_vexpand (audio_box, TRUE);
		gtk_widget_set_hexpand (audio_box, TRUE);
		gtk_widget_set_valign (audio_box, GTK_ALIGN_CENTER);
		slot->preview_audio_box = audio_box;

		/* Audio icon */
		audio_icon = gtk_image_new_from_icon_name ("audio-x-generic", GTK_ICON_SIZE_DIALOG);
		gtk_image_set_pixel_size (GTK_IMAGE (audio_icon), 96);
		gtk_widget_set_halign (audio_icon, GTK_ALIGN_CENTER);
		slot->preview_audio_icon = audio_icon;
		gtk_box_pack_start (GTK_BOX (audio_box), audio_icon, FALSE, FALSE, 0);

		/* Controls box */
		controls_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign (controls_box, GTK_ALIGN_CENTER);
		gtk_widget_set_margin_top (controls_box, 12);

		/* Play/Pause button */
		play_button = gtk_button_new ();
		play_image = gtk_image_new_from_icon_name ("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (play_button), play_image);
		gtk_button_set_relief (GTK_BUTTON (play_button), GTK_RELIEF_NONE);
		gtk_widget_set_can_focus (play_button, FALSE);
		slot->preview_audio_play_btn = play_button;

		g_signal_connect (play_button, "clicked",
				  G_CALLBACK (on_audio_play_button_clicked), slot);

		gtk_box_pack_start (GTK_BOX (controls_box), play_button, FALSE, FALSE, 0);

		/* Timeline slider */
		timeline = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
		gtk_scale_set_draw_value (GTK_SCALE (timeline), FALSE);
		gtk_widget_set_size_request (timeline, 150, -1);
		slot->preview_audio_timeline = timeline;

		g_signal_connect (timeline, "value-changed",
				  G_CALLBACK (on_audio_timeline_value_changed), slot);

		gtk_box_pack_start (GTK_BOX (controls_box), timeline, FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (audio_box), controls_box, FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (box), audio_box, TRUE, TRUE, 0);
	}
#endif

#ifdef HAVE_POPPLER
	/* PDF preview */
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

	/* Label for filename */
	label = gtk_label_new (_("No file selected"));
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 30);
	gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
	slot->preview_label = label;

	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	gtk_widget_show_all (box);

	/* Start with image preview visible, others hidden */
	gtk_widget_hide (text_scroll);
#ifdef HAVE_GSTREAMER
	gtk_widget_hide (slot->preview_video_box);
	gtk_widget_hide (slot->preview_video_error_box);
	gtk_widget_hide (slot->preview_audio_box);
#endif
#ifdef HAVE_POPPLER
	gtk_widget_hide (slot->preview_pdf_scroll);
#endif

	return box;
}

void
nemo_window_slot_set_preview_visible (NemoWindowSlot *slot, gboolean visible)
{
	g_return_if_fail (NEMO_IS_WINDOW_SLOT (slot));

	if (slot->preview_visible == visible) {
		return;
	}

	slot->preview_visible = visible;

	if (slot->preview_pane != NULL) {
		gtk_widget_set_visible (slot->preview_pane, visible);

		if (visible && slot->content_view != NULL) {
			/* Update preview with current selection */
			on_selection_changed (slot->content_view, slot);
		}
	}
}

gboolean
nemo_window_slot_get_preview_visible (NemoWindowSlot *slot)
{
	g_return_val_if_fail (NEMO_IS_WINDOW_SLOT (slot), FALSE);

	return slot->preview_visible;
}

static void
nemo_window_slot_init (NemoWindowSlot *slot)
{
	GtkWidget *extras_vbox;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	slot->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->query_editor = NEMO_QUERY_EDITOR (nemo_query_editor_new ());

	nemo_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->query_editor));

	/* Create the horizontal paned for content + preview */
	slot->content_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (slot), slot->content_paned, TRUE, TRUE, 0);
	gtk_widget_show (slot->content_paned);

	/* Create view overlay (goes in left/first child of paned) */
	slot->view_overlay = gtk_overlay_new ();
	gtk_widget_add_events (slot->view_overlay,
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	gtk_paned_pack1 (GTK_PANED (slot->content_paned), slot->view_overlay, TRUE, FALSE);
	gtk_widget_show (slot->view_overlay);

	slot->floating_bar = nemo_floating_bar_new ("", FALSE);
	gtk_widget_set_halign (slot->floating_bar, GTK_ALIGN_END);
	gtk_widget_set_valign (slot->floating_bar, GTK_ALIGN_END);
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
				 slot->floating_bar);

	slot->no_search_results_box = create_nsr_box ();
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
				 slot->no_search_results_box);

	g_signal_connect (slot->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

	/* Create preview pane (goes in right/second child of paned) */
	slot->preview_pane = create_preview_pane (slot);
	gtk_paned_pack2 (GTK_PANED (slot->content_paned), slot->preview_pane, FALSE, FALSE);

	/* Start with preview hidden */
	slot->preview_visible = FALSE;
	gtk_widget_hide (slot->preview_pane);

	slot->cache_bar = NULL;
	slot->selection_changed_id = 0;

	slot->title = g_strdup (_("Loading..."));
}

static void
view_end_loading_cb (NemoView       *view,
		     		 gboolean        all_files_seen,
		     		 NemoWindowSlot *slot)
{
	if (slot->needs_reload) {
		nemo_window_slot_queue_reload (slot, FALSE);
		slot->needs_reload = FALSE;
	} else if (all_files_seen) {
        NemoDirectory *directory;

        directory = nemo_directory_get_for_file (slot->viewed_file);

        if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
            if (!nemo_directory_is_not_empty (directory)) {
                gtk_widget_show (slot->no_search_results_box);
            } else {
                gtk_widget_hide (slot->no_search_results_box);

            }
        }

        nemo_directory_unref (directory);
    }
}

static void
nemo_window_slot_dispose (GObject *object)
{
	NemoWindowSlot *slot;
	GtkWidget *widget;

	slot = NEMO_WINDOW_SLOT (object);

#ifdef HAVE_GSTREAMER
	/* Stop video playback and clean up pipeline */
	stop_video_pipeline (slot);
	/* Stop audio playback and clean up pipeline */
	stop_audio_pipeline (slot);
#endif

	/* Stop any running GIF animation */
	stop_gif_animation (slot);

	nemo_window_slot_clear_forward_list (slot);
	nemo_window_slot_clear_back_list (slot);
    nemo_window_slot_remove_extra_location_widgets (slot);

	if (slot->content_view) {
		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (slot->new_content_view) {
		widget = GTK_WIDGET (slot->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->new_content_view);
		slot->new_content_view = NULL;
	}

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	if (slot->loading_timeout_id != 0) {
		g_source_remove (slot->loading_timeout_id);
		slot->loading_timeout_id = 0;
	}

	nemo_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
	 * It was already here before the slot move, though */
	nemo_file_unref (slot->viewed_file);

	if (slot->location) {
		/* TODO? why do we ref here, instead of unreffing?
		 * It was already here before the slot migration, though */
		g_object_ref (slot->location);
	}

	g_list_free_full (slot->pending_selection, g_object_unref);
	slot->pending_selection = NULL;

	g_clear_object (&slot->current_location_bookmark);
	g_clear_object (&slot->last_location_bookmark);

	if (slot->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->find_mount_cancellable);
		slot->find_mount_cancellable = NULL;
	}

	slot->pane = NULL;

	g_free (slot->title);
	slot->title = NULL;

	g_free (slot->status_text);
	slot->status_text = NULL;

	G_OBJECT_CLASS (nemo_window_slot_parent_class)->dispose (object);
}

static void
nemo_window_slot_class_init (NemoWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nemo_window_slot_dispose;

	signals[ACTIVE] =
		g_signal_new ("active",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoWindowSlotClass, active),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[INACTIVE] =
		g_signal_new ("inactive",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoWindowSlotClass, inactive),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[CHANGED_PANE] =
		g_signal_new ("changed-pane",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (NemoWindowSlotClass, changed_pane),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] =
		g_signal_new ("location-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);
}

GFile *
nemo_window_slot_get_location (NemoWindowSlot *slot)
{
	g_assert (slot != NULL);

	if (slot->location != NULL) {
		return g_object_ref (slot->location);
	}
	return NULL;
}

char *
nemo_window_slot_get_location_uri (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	if (slot->location) {
		return g_file_get_uri (slot->location);
	}
	return NULL;
}

void
nemo_window_slot_make_hosting_pane_active (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_PANE (slot->pane));

	nemo_window_set_active_slot (nemo_window_slot_get_window (slot),
					 slot);
}

NemoWindow *
nemo_window_slot_get_window (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	return slot->pane->window;
}

/* nemo_window_slot_update_title:
 *
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NemoWindowSlot in question.
 *
 */
void
nemo_window_slot_update_title (NemoWindowSlot *slot)
{
	NemoWindow *window;
	char *title;
	gboolean do_sync = FALSE;

	title = nemo_compute_title_for_location (slot->location);
	window = nemo_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->title) != 0) {
		do_sync = TRUE;

		g_free (slot->title);
		slot->title = title;
		title = NULL;
	}

	if (strlen (slot->title) > 0 &&
	    slot->current_location_bookmark != NULL) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nemo_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

/* nemo_window_slot_update_icon:
 *
 * Re-calculate the slot icon
 * Called when the location or view or icon set has changed.
 * @slot: The NemoWindowSlot in question.
 */
void
nemo_window_slot_update_icon (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoIconInfo *info;
	const char *icon_name;
	GdkPixbuf *pixbuf;

	window = nemo_window_slot_get_window (slot);
	info = NEMO_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->get_icon (window, slot);

	icon_name = NULL;
	if (info) {
		icon_name = nemo_icon_info_get_used_name (info);
		if (icon_name != NULL) {
			/* Gtk+ doesn't short circuit this (yet), so avoid lots of work
			 * if we're setting to the same icon. This happens a lot e.g. when
			 * the trash directory changes due to the file count changing.
			 */
			if (g_strcmp0 (icon_name, gtk_window_get_icon_name (GTK_WINDOW (window))) != 0) {
				gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
			}
		} else {
			pixbuf = nemo_icon_info_get_pixbuf_nodefault (info);

			if (pixbuf) {
				gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
				g_object_unref (pixbuf);
			}
		}

        nemo_icon_info_unref (info);
	}
}

void
nemo_window_slot_set_show_thumbnails (NemoWindowSlot *slot,
                                      gboolean show_thumbnails)
{
  NemoDirectory *directory;

  directory = nemo_directory_get (slot->location);
  nemo_directory_set_show_thumbnails(directory, show_thumbnails);
  nemo_directory_unref (directory);
}

void
nemo_window_slot_set_content_view_widget (NemoWindowSlot *slot,
					      NemoView *new_view)
{
	NemoWindow *window;
	GtkWidget *widget;

	window = nemo_window_slot_get_window (slot);

	if (slot->content_view != NULL) {
		/* disconnect old view */
		g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_end_loading_cb), slot);

		/* disconnect selection-changed for preview pane */
		if (slot->selection_changed_id > 0) {
			g_signal_handler_disconnect (slot->content_view, slot->selection_changed_id);
			slot->selection_changed_id = 0;
		}

		nemo_window_disconnect_content_view (window, slot->content_view);

		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (new_view != NULL) {
		widget = GTK_WIDGET (new_view);
		gtk_container_add (GTK_CONTAINER (slot->view_overlay), widget);
		gtk_widget_show (widget);

		slot->content_view = new_view;
		g_object_ref (slot->content_view);

		g_signal_connect (new_view, "end_loading", G_CALLBACK (view_end_loading_cb), slot);

		/* connect selection-changed for preview pane */
		slot->selection_changed_id = g_signal_connect (new_view, "selection-changed",
							       G_CALLBACK (on_selection_changed), slot);

		/* connect new view */
		nemo_window_connect_content_view (window, new_view);
	}
}

void
nemo_window_slot_set_allow_stop (NemoWindowSlot *slot,
				     gboolean allow)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	slot->allow_stop = allow;

	window = nemo_window_slot_get_window (slot);
	nemo_window_sync_allow_stop (window, slot);
}

static void
real_slot_set_short_status (NemoWindowSlot *slot,
			    const gchar *status)
{

	gboolean show_statusbar;
	gboolean disable_chrome;

	nemo_floating_bar_cleanup_actions (NEMO_FLOATING_BAR (slot->floating_bar));
	nemo_floating_bar_set_show_spinner (NEMO_FLOATING_BAR (slot->floating_bar),
						FALSE);

	show_statusbar = g_settings_get_boolean (nemo_window_state,
						 NEMO_WINDOW_STATE_START_WITH_STATUS_BAR);

	g_object_get (nemo_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (status == NULL || show_statusbar || disable_chrome) {
		gtk_widget_hide (slot->floating_bar);
		return;
	}

	nemo_floating_bar_set_label (NEMO_FLOATING_BAR (slot->floating_bar), status);
	gtk_widget_show (slot->floating_bar);
}

typedef struct {
	gchar *status;
	NemoWindowSlot *slot;
} SetStatusData;

static void
set_status_data_free (gpointer data)
{
	SetStatusData *status_data = data;

	g_free (status_data->status);

	g_free (data);
}

static gboolean
set_status_timeout_cb (gpointer data)
{
	SetStatusData *status_data = data;

	status_data->slot->set_status_timeout_id = 0;
	real_slot_set_short_status (status_data->slot, status_data->status);

	return FALSE;
}

static void
set_floating_bar_status (NemoWindowSlot *slot,
			 const gchar *status)
{
	GtkSettings *settings;
	gint double_click_time;
	SetStatusData *status_data;

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (slot->content_view)));
	g_object_get (settings,
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	status_data = g_new0 (SetStatusData, 1);
	status_data->status = g_strdup (status);
	status_data->slot = slot;

	/* waiting for half of the double-click-time before setting
	 * the status seems to be a good approximation of not setting it
	 * too often and not delaying the statusbar too much.
	 */
	slot->set_status_timeout_id =
		g_timeout_add_full (G_PRIORITY_DEFAULT,
				    (guint) (double_click_time / 2),
				    set_status_timeout_cb,
				    status_data,
				    set_status_data_free);
}

void
nemo_window_slot_set_status (NemoWindowSlot *slot,
                             const char *status,
                             const char *short_status,
                             gboolean    location_loading)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_free (slot->status_text);
	slot->status_text = g_strdup (status);

	if (slot->content_view != NULL && !location_loading) {
		set_floating_bar_status (slot, short_status);
	}

	window = nemo_window_slot_get_window (slot);
	if (slot == nemo_window_get_active_slot (window)) {
		nemo_window_push_status (window, slot->status_text);
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NemoWindowSlot *slot = data;
	NemoDirectory *directory;

	directory = nemo_directory_get (slot->location);
	if (widget != GTK_WIDGET (slot->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->extra_location_widgets), widget);
	}

	nemo_directory_unref (directory);
}

void
nemo_window_slot_remove_extra_location_widgets (NemoWindowSlot *slot)
{
    gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
                           remove_all_extra_location_widgets,
                           slot);
}

void
nemo_window_slot_add_extra_location_widget (NemoWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->extra_location_widgets);
}

/* returns either the pending or the actual current uri */
char *
nemo_window_slot_get_current_uri (NemoWindowSlot *slot)
{
	if (slot->pending_location != NULL) {
		return g_file_get_uri (slot->pending_location);
	}

	if (slot->location != NULL) {
		return g_file_get_uri (slot->location);
	}

	g_assert_not_reached ();
	return NULL;
}

NemoView *
nemo_window_slot_get_current_view (NemoWindowSlot *slot)
{
	if (slot->content_view != NULL) {
		return slot->content_view;
	} else if (slot->new_content_view) {
		return slot->new_content_view;
	}

	return NULL;
}

void
nemo_window_slot_go_home (NemoWindowSlot *slot,
			      NemoWindowOpenFlags flags)
{
	GFile *home;

	g_return_if_fail (NEMO_IS_WINDOW_SLOT (slot));

	home = g_file_new_for_path (g_get_home_dir ());
	nemo_window_slot_open_location (slot, home, flags);
	g_object_unref (home);
}

void
nemo_window_slot_go_up (NemoWindowSlot *slot,
			    NemoWindowOpenFlags flags)
{
	GFile *parent;
	char * uri;

	if (slot->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->location);
	if (parent == NULL) {
		if (g_file_has_uri_scheme (slot->location, "smb")) {
			uri = g_file_get_uri (slot->location);

            DEBUG ("Starting samba URI for navigation: %s", uri);

			if (g_strcmp0 ("smb:///", uri) == 0) {
				parent = g_file_new_for_uri ("network:///");
			}
			else {
                GString *gstr;
                char * temp;

                gstr = g_string_new (uri);

				// Remove last /
                if (g_str_has_suffix (gstr->str, "/")) {
                    gstr = g_string_set_size (gstr, gstr->len - 1);
                }

				// Remove last part of string after last remaining /
				temp = g_strrstr (gstr->str, "/") + 1;
				if (temp != NULL) {
                    gstr = g_string_set_size (gstr, temp - gstr->str);
				}

                // if we're going to end up with smb://, redirect it to network instead.
                if (g_strcmp0 ("smb://", gstr->str) == 0) {
                    gstr = g_string_assign (gstr, "network:///");
                }

                uri = g_string_free (gstr, FALSE);

				parent = g_file_new_for_uri (uri);

                DEBUG ("Ending samba URI for navigation: %s", uri);
			}
			g_free (uri);
		}
		else {
			return;
		}
	}

	nemo_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

void
nemo_window_slot_clear_forward_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->forward_list, g_object_unref);
	slot->forward_list = NULL;
}

void
nemo_window_slot_clear_back_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->back_list, g_object_unref);
	slot->back_list = NULL;
}

gboolean
nemo_window_slot_should_close_with_mount (NemoWindowSlot *slot,
					      GMount *mount)
{
	GFile *mount_location;
	gboolean close_with_mount;

	mount_location = g_mount_get_root (mount);
	close_with_mount =
		g_file_has_prefix (NEMO_WINDOW_SLOT (slot)->location, mount_location) ||
		g_file_equal (NEMO_WINDOW_SLOT (slot)->location, mount_location);

	g_object_unref (mount_location);

	return close_with_mount;
}

NemoWindowSlot *
nemo_window_slot_new (NemoWindowPane *pane)
{
	NemoWindowSlot *slot;

	slot = g_object_new (NEMO_TYPE_WINDOW_SLOT, NULL);
	slot->pane = pane;

	return slot;
}
