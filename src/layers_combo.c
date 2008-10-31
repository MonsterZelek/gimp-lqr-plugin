/* GIMP LiquidRescaling Plug-in
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * All Rights Reserved.
 *
 * This plugin implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
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

#include "config.h"

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include <lqr.h>

#include "plugin-intl.h"
#include "main.h"
#include "preview.h"
#include "layers_combo.h"

extern int context_calls;
extern gboolean pres_info_show;
extern gboolean disc_info_show;
extern gboolean rigmask_info_show;
extern GtkWidget * dlg;


gint
count_extra_layers (gint32 image_ID, GimpDrawable * drawable)
{
  gint32 *layer_array;
  gint num_layers;

  layer_array = gimp_image_get_layers (image_ID, &num_layers);
  return num_layers - 1;
}

gboolean
dialog_layer_constraint_func (gint32 image_id, gint32 layer_id, gpointer data)
{
  GimpDrawable * gdrawable = (GimpDrawable *) data;
  if (image_id !=
      gimp_drawable_get_image (gdrawable->drawable_id))
    {
      return FALSE;
    }
  if (layer_id == gdrawable->drawable_id)
    {
      return FALSE;
    }
  return TRUE;
}

void
callback_pres_combo_get_active (GtkWidget * combo, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);

  gint32 *layer_ID_add = &(p_data->vals->pres_layer_ID);
  gboolean status = p_data->ui_vals->pres_status;
  guchar ** buffer_add = &(p_data->pres_buffer);

  combo_get_active (combo, p_data, layer_ID_add, status, buffer_add);
}

void
callback_disc_combo_get_active (GtkWidget * combo, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);

  gint32 *layer_ID_add = &(p_data->vals->disc_layer_ID);
  gboolean status = p_data->ui_vals->disc_status;
  guchar **buffer_add = &(p_data->disc_buffer);

  combo_get_active (combo, p_data, layer_ID_add, status, buffer_add);
}

void
callback_rigmask_combo_get_active (GtkWidget * combo, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);

  gint32 *layer_ID_add = &(p_data->vals->rigmask_layer_ID);
  gboolean status = p_data->ui_vals->rigmask_status;
  guchar **buffer_add = &(p_data->rigmask_buffer);

  combo_get_active (combo, p_data, layer_ID_add, status, buffer_add);
}

void
combo_get_active (GtkWidget * combo, PreviewData * p_data,
		  gint32 * layer_ID_add, gboolean status,
		  guchar ** buffer_add)
{
  gint32 new_layer_ID;
  gint x_off, y_off;

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (combo), layer_ID_add);
  if (status == TRUE)
    {
      gimp_image_undo_freeze (p_data->image_ID);
      new_layer_ID = gimp_layer_copy (*layer_ID_add);
      gimp_image_add_layer (p_data->image_ID, new_layer_ID, -1);
      gimp_drawable_offsets (new_layer_ID, &x_off, &y_off);
      gimp_layer_resize (new_layer_ID, p_data->old_width,
			 p_data->old_height,
			 x_off - p_data->x_off, y_off - p_data->y_off);
      gimp_layer_scale (new_layer_ID, p_data->width, p_data->height, TRUE);
      gimp_layer_add_alpha (new_layer_ID);
      g_free (*buffer_add);
      (*buffer_add) = preview_build_buffer (new_layer_ID);
      gimp_image_remove_layer (p_data->image_ID, new_layer_ID);
      gimp_image_undo_thaw (p_data->image_ID);

    }
  preview_build_pixbuf (p_data);
  gtk_widget_queue_draw (p_data->area);
}


void
callback_combo_set_sensitive (GtkWidget * button, gpointer data)
{
  ToggleData *t_data = TOGGLE_DATA (data);
  gboolean button_status =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gtk_widget_set_sensitive (t_data->combo, button_status);
  gtk_widget_set_sensitive (t_data->combo_label,
			    button_status);
  if (t_data->scale)
    {
      gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_LABEL
				(t_data->scale), button_status);
      gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SCALE
				(t_data->scale), button_status);
      gtk_widget_set_sensitive (GIMP_SCALE_ENTRY_SPINBUTTON
				(t_data->scale), button_status);
    }
  if (t_data->guess_button)
    {
      gtk_widget_set_sensitive (t_data->guess_button, button_status);
      gtk_widget_set_sensitive (t_data->guess_dir_combo, button_status);
    }
  *(t_data->status) = button_status;
}

void
callback_status_button (GtkWidget * button, gpointer data)
{
  gboolean button_status =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  *((gboolean *) (data)) = button_status;
}

void
callback_new_mask_button (GtkWidget * button, gpointer data)
{
  gint32 layer_ID;
  NewLayerData *nl_data = NEW_LAYER_DATA (data);
  PreviewData *p_data = nl_data->preview_data;
  GimpImageType image_type;
  GimpRGB *fg_colour;
  GimpRGB grey;


  switch (gimp_image_base_type (p_data->image_ID))
    {
      case GIMP_RGB:
        image_type = GIMP_RGBA_IMAGE;
        fg_colour = &(nl_data->colour);
        break;
      case GIMP_GRAY:
        image_type = GIMP_GRAYA_IMAGE;
        gimp_rgb_set(&grey, 1.0 / 3, 1.0 / 3, 1.0 / 3);
        fg_colour = &grey;
        break;
      default:
        return;
    }

  gimp_image_undo_group_start (p_data->image_ID);
  layer_ID =
    gimp_layer_new (p_data->image_ID, nl_data->name,
		    p_data->old_width, p_data->old_height,
		    image_type, 50, GIMP_NORMAL_MODE);
  gimp_image_add_layer (p_data->image_ID, layer_ID, -1);
  gimp_drawable_fill (layer_ID, GIMP_TRANSPARENT_FILL);
  gimp_layer_translate (layer_ID, p_data->x_off, p_data->y_off);
  gimp_image_undo_group_end (p_data->image_ID);
  *(nl_data->layer_ID) = layer_ID;
  *(nl_data->status) = TRUE;
  context_calls++;
  gimp_context_set_foreground (fg_colour);

  pres_info_show = FALSE;
  disc_info_show = FALSE;
  rigmask_info_show = FALSE;

  *(nl_data->info_show) = TRUE;

  if (nl_data->presdisc == TRUE)
    {
      gtk_dialog_response (GTK_DIALOG (dlg), RESPONSE_FEAT_REFRESH);
    }
  else
    {
      gtk_dialog_response (GTK_DIALOG (dlg), RESPONSE_ADV_REFRESH);
    }

  *(nl_data->info_show) = FALSE;
}


void
callback_guess_button (GtkWidget * button, gpointer data)
{
  gint new_width, new_height;
  PreviewData *p_data = PREVIEW_DATA (data);

  new_width = p_data->old_width;
  new_height = p_data->old_height;
  switch (p_data->ui_vals->guess_direction)
    {
    case 0:
      new_width = guess_new_width (button, data);
      break;
    case 1:
      new_height = guess_new_height (button, data);
      break;
    default:
      g_message ("You just found a bug");
    }

  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY
			      (p_data->coordinates), 0, new_width);
  gimp_size_entry_set_refval (GIMP_SIZE_ENTRY
			      (p_data->coordinates), 1, new_height);
}

void
callback_guess_direction (GtkWidget * combo, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);

  gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX (combo),
				 &(p_data->ui_vals->guess_direction));
}

gint
guess_new_width (GtkWidget * button, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);
  return guess_new_size (button, p_data, GUESS_DIR_HOR);
}

gint
guess_new_height (GtkWidget * button, gpointer data)
{
  PreviewData *p_data = PREVIEW_DATA (data);
  return guess_new_size (button, p_data, GUESS_DIR_VERT);
}

gint
guess_new_size (GtkWidget * button, PreviewData * p_data, GuessDir direction)
{
  gint32 disc_layer_ID;
  GimpDrawable *drawable;
  gint z1, z2, k;
  gint z1min, z1max, z2max;
  gint width, height;
  gint lw, lh;
  gint x_off, y_off;
  gint bpp, c_bpp;
  GimpPixelRgn rgn_in;
  guchar *line;
  gboolean has_alpha;
  gdouble sum;
  gint mask_size;
  gint max_mask_size = 0;
  gint old_size;
  gint new_size;

  disc_layer_ID = p_data->vals->disc_layer_ID;
  switch (direction)
    {
      case GUESS_DIR_HOR:
        old_size = p_data->old_width;
        break;
      case GUESS_DIR_VERT:
        old_size = p_data->old_height;
        break;
      default:
        g_message("You just found a bug");
        return 0;
    }

  if (!gimp_drawable_is_valid (disc_layer_ID))
    {
      g_message (_("Error: it seems that the selected layer "
		   "is no longer valid"));
      return old_size;	/* Should refresh here */
    }

  width = gimp_drawable_width (disc_layer_ID);
  height = gimp_drawable_height (disc_layer_ID);
  has_alpha = gimp_drawable_has_alpha (disc_layer_ID);
  bpp = gimp_drawable_bpp (disc_layer_ID);
  c_bpp = bpp - (has_alpha ? 1 : 0);

  drawable = gimp_drawable_get (disc_layer_ID);
  gimp_pixel_rgn_init (&rgn_in, drawable, 0, 0, width, height, FALSE, FALSE);


  gimp_drawable_offsets (disc_layer_ID, &x_off, &y_off);

  x_off -= p_data->x_off;
  y_off -= p_data->y_off;

  lw = (MIN (p_data->old_width, width + x_off) - MAX (0, x_off));
  lh = (MIN (p_data->old_height, height + y_off) - MAX (0, y_off));

  switch (direction)
    {
      case GUESS_DIR_HOR:
        z1min = MAX (0, y_off);
        z1max = MIN (p_data->old_height, height + y_off);
        z2max = lw;
        break;
      case GUESS_DIR_VERT:
        z1min = MAX (0, x_off);
        z1max = MIN (p_data->old_width, width + x_off);
        z2max = lh;
        break;
      default:
        g_message("You just found a bug");
        return 0;
    }

  line = g_try_new (guchar, bpp * z2max);

  for (z1 = z1min; z1 < z1max; z1++)
    {
      switch (direction)
        {
          case GUESS_DIR_HOR:
            gimp_pixel_rgn_get_row (&rgn_in, line, MAX (0, -x_off), z1 - y_off, z2max);
            break;
          case GUESS_DIR_VERT:
            gimp_pixel_rgn_get_col (&rgn_in, line, z1 - x_off, MAX (0, -y_off), z2max);
            break;
        }

      mask_size = 0;
      for (z2 = 0; z2 < z2max; z2++)
	{
	  sum = 0;
	  for (k = 0; k < c_bpp; k++)
	    {
	      sum += line[bpp * z2 + k];
	    }

	  sum /= (255 * c_bpp);
	  if (has_alpha)
	    {
	      sum *= (gdouble) line[bpp * (z2 + 1) - 1] / 255;
	    }

	  if (sum >= (0.5 / c_bpp))
	    {
	      mask_size++;
	    }
	}
      if (mask_size > max_mask_size)
	{
	  max_mask_size = mask_size;
	}

    }

  new_size = old_size - max_mask_size;

  g_free (line);
  gimp_drawable_detach (drawable);

  return new_size;
}


