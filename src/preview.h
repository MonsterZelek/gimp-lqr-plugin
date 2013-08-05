/* GIMP LiquidRescale Plug-in
 * Copyright (C) 2007-2010 Carlo Baldassi (the "Author") <carlobaldassi@gmail.com>.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the Licence, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org.licences/>.
 */


#ifndef __PREVIEW_H__
#define __PREVIEW_H__


/*  Constants  */

#define PREVIEW_MAX_WIDTH  300
#define PREVIEW_MAX_HEIGHT 200

typedef struct
{
  gint x_off;
  gint y_off;
  gint width;
  gint height;
} SizeInfo;

/*  Preview data struct */

typedef struct
{
  gint32 image_ID;
  gint32 orig_layer_ID;
  gint32 layer_ID;
  GimpImageType type;
  PlugInVals *vals;
  PlugInUIVals *ui_vals;
  gint width;
  gint height;
  gint old_width;
  gint old_height;
  gfloat factor;
  gint x_off;
  gint y_off;
  SizeInfo pres_size_info;
  SizeInfo disc_size_info;
  SizeInfo rigmask_size_info;
  GdkPixbuf *base_pixbuf;
  GdkPixbuf *pres_pixbuf;
  GdkPixbuf *disc_pixbuf;
  GdkPixbuf *rigmask_pixbuf;
  GdkPixbuf *pixbuf;
  GtkWidget *dlg;
  GtkWidget *area;
  GtkWidget *pres_combo;
  GtkWidget *disc_combo;
  GtkWidget *rigmask_combo;
  gboolean pres_combo_awaked;
  gboolean disc_combo_awaked;
  gboolean rigmask_combo_awaked;
  GtkWidget *coordinates;
  GtkWidget *disc_warning_image;
  GtkWidget *pres_use_image;
  GtkWidget *disc_use_image;
  GtkWidget *rigmask_use_image;

} PreviewData;

#define PREVIEW_DATA(data) ((PreviewData*)data)


/*  Functions  */

void size_info_scale(SizeInfo * size_info, gdouble factor);

void callback_pres_combo_set_sensitive_preview (GtkWidget * button,
                                                       gpointer data);
void callback_disc_combo_set_sensitive_preview (GtkWidget * button,
                                                       gpointer data);
void callback_rigmask_combo_set_sensitive_preview (GtkWidget * button,
                                                          gpointer data);

void preview_init_mem (PreviewData * preview_data);
void preview_data_create(gint32 image_ID, gint32 layer_ID, PreviewData * p_data);
GtkWidget * preview_area_create(PreviewData * p_data);
void preview_build_pixbuf (PreviewData * preview_data);

void
callback_preview_expose_event (GtkWidget * preview_area,
                               GdkEventExpose * event, gpointer data);

void
update_info_aux_use_icons(PlugInVals *vals, PlugInUIVals *ui_vals,
    GtkWidget *pres_use_image, GtkWidget *disc_use_image, GtkWidget *rigmask_use_image);

#endif /* __PREVIEW_H__ */
