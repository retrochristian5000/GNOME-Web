/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2026 Cleo Menezes Jr. <menezesjr@pm.me>
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-screenshot-capture.h"

#include <math.h>

#include "ephy-debug.h"

#define CROP_FORMAT GDK_MEMORY_R8G8B8A8_PREMULTIPLIED
#define CROP_BYTES_PER_PIXEL 4

typedef struct {
  EphyScreenshotCrop crop;
  gboolean has_crop;
} CaptureData;

static GdkTexture *
crop_texture (GdkTexture               *texture,
              const EphyScreenshotCrop *crop)
{
  g_autoptr (GdkTextureDownloader) downloader = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GBytes) cropped_bytes = NULL;
  gsize stride = 0;
  int texture_width = gdk_texture_get_width (texture);
  int texture_height = gdk_texture_get_height (texture);
  double scale;
  int x;
  int y;
  int width;
  int height;
  int requested_width;
  int requested_height;

  scale = crop->region_width > 0 ? texture_width / crop->region_width : 1.0;

  x = CLAMP ((int)floor (crop->x * scale), 0, texture_width - 1);
  y = CLAMP ((int)floor (crop->y * scale), 0, texture_height - 1);
  requested_width = (int)ceil (crop->width * scale);
  requested_height = (int)ceil (crop->height * scale);
  width = CLAMP (requested_width, 1, texture_width - x);
  height = CLAMP (requested_height, 1, texture_height - y);

  /* Clipping here means the caller asked for something outside the snapshot,
   * which otherwise shows up as a blank preview.
   */
  if (width < requested_width || height < requested_height)
    LOG ("Crop %dx%d at %d,%d clipped to %dx%d by a %dx%d snapshot",
         requested_width, requested_height, x, y, width, height,
         texture_width, texture_height);

  if (x == 0 && y == 0 && width == texture_width && height == texture_height)
    return g_object_ref (texture);

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, CROP_FORMAT);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  /* Rows of the cropped area are not contiguous, so keep the original stride
   * and point at the first pixel of the first row instead of copying.
   */
  cropped_bytes = g_bytes_new_from_bytes (bytes,
                                          (gsize)y * stride + (gsize)x * CROP_BYTES_PER_PIXEL,
                                          (gsize)(height - 1) * stride + (gsize)width * CROP_BYTES_PER_PIXEL);

  return gdk_memory_texture_new (width, height, CROP_FORMAT, cropped_bytes, stride);
}

static void
snapshot_ready_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  g_autoptr (GdkTexture) texture = NULL;
  GError *error = NULL;
  CaptureData *data = g_task_get_task_data (task);

  texture = webkit_web_view_get_snapshot_finish (WEBKIT_WEB_VIEW (source), result, &error);
  if (!texture) {
    g_task_return_error (task, error);
    return;
  }

  if (data->has_crop)
    g_task_return_pointer (task, crop_texture (texture, &data->crop), g_object_unref);
  else
    g_task_return_pointer (task, g_steal_pointer (&texture), g_object_unref);
}

void
ephy_screenshot_capture_async (EphyWebView              *view,
                               EphyScreenshotRegion      region,
                               const EphyScreenshotCrop *crop,
                               GCancellable             *cancellable,
                               GAsyncReadyCallback       callback,
                               gpointer                  user_data)
{
  GTask *task;
  CaptureData *data;
  WebKitSnapshotRegion snapshot_region;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  task = g_task_new (view, cancellable, callback, user_data);
  g_task_set_source_tag (task, ephy_screenshot_capture_async);

  data = g_new0 (CaptureData, 1);
  if (crop) {
    data->crop = *crop;
    data->has_crop = TRUE;
  }
  g_task_set_task_data (task, data, g_free);

  snapshot_region = region == EPHY_SCREENSHOT_REGION_FULL_PAGE ? WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT
                                                               : WEBKIT_SNAPSHOT_REGION_VISIBLE;

  webkit_web_view_get_snapshot (WEBKIT_WEB_VIEW (view),
                                snapshot_region,
                                WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                cancellable,
                                snapshot_ready_cb,
                                task);
}

GdkTexture *
ephy_screenshot_capture_finish (EphyWebView   *view,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, view), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
