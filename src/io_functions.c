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

#include <stdio.h>

#include <libgimp/gimp.h>
#include <lqr.h>

#include "config.h"
#include "plugin-intl.h"

#include "io_functions.h"

guchar *
rgb_buffer_from_layer (gint32 layer_ID)
{
  gint y, bpp;
  gint w, h;
  GimpDrawable *drawable;
  GimpPixelRgn rgn_in;
  guchar *buffer;
  gint update_step;

  gimp_progress_init (_("Parsing layer..."));

  w = gimp_drawable_width (layer_ID);
  h = gimp_drawable_height (layer_ID);

  bpp = gimp_drawable_bpp (layer_ID);

  TRY_N_N (buffer = g_try_new (guchar, bpp * w * h));

  drawable = gimp_drawable_get (layer_ID);

  gimp_pixel_rgn_init (&rgn_in, drawable, 0, 0, w, h, FALSE, FALSE);

  for (y = 0; y < h; y++)
    {
      gimp_pixel_rgn_get_row (&rgn_in, buffer + y * w * bpp, 0, y, w);

      update_step = MAX ((h - 1) / 20, 1);
      if (y % update_step == 0)
        {
          gimp_progress_update ((gdouble) y / (h - 1));
        }
    }

  gimp_drawable_detach (drawable);

  return buffer;
}

LqrRetVal
update_bias (LqrCarver * r, gint32 layer_ID, gint bias_factor,
             gint base_x_off, gint base_y_off)
{
  guchar *rgb;
  gint w, h, bpp;
  gint x_off, y_off;

  if ((layer_ID == 0) || (bias_factor == 0))
    {
      return LQR_OK;
    }

  gimp_drawable_offsets (layer_ID, &x_off, &y_off);
  x_off -= base_x_off;
  y_off -= base_y_off;

  w = gimp_drawable_width (layer_ID);
  h = gimp_drawable_height (layer_ID);

  bpp = gimp_drawable_bpp (layer_ID);

  rgb = rgb_buffer_from_layer (layer_ID);

  CATCH (lqr_carver_bias_add_rgb_area
         (r, rgb, bias_factor, bpp, w, h, x_off, y_off));

  return LQR_OK;
}

LqrRetVal
write_carver_to_layer (LqrCarver * r, GimpDrawable * drawable)
{
  gint32 layer_ID;
  gint y;
  gint w, h;
  GimpPixelRgn rgn_out;
  guchar *out_line;
  gint update_step;

  gimp_progress_init (_("Applying changes..."));
  update_step = MAX ((r->h - 1) / 20, 1);

  layer_ID = drawable->drawable_id;

  w = gimp_drawable_width (layer_ID);
  h = gimp_drawable_height (layer_ID);

  gimp_pixel_rgn_init (&rgn_out, drawable, 0, 0, w, h, TRUE, TRUE);


  while (lqr_carver_scan_line (r, &y, &out_line))
    {
      if (!r->transposed)
        {
          gimp_pixel_rgn_set_row (&rgn_out, out_line, 0, y, w);
        }
      else
        {
          gimp_pixel_rgn_set_col (&rgn_out, out_line, y, 0, h);
        }

      if (y % update_step == 0)
        {
          gimp_progress_update ((gdouble) y / (r->h - 1));
        }

    }

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (layer_ID, TRUE);
  gimp_drawable_update (layer_ID, 0, 0, w, h);

  return LQR_OK;
}

LqrRetVal
write_vmap_to_layer (LqrVMap * vmap, gpointer data)
{
  gint w, h, bpp;
  gint depth;
  gint *buffer;
  gint32 seam_layer_ID;
  gint32 image_ID;
  GimpDrawable *drawable;
  gint x_off, y_off;
  gchar *name;
  GimpRGB col_start, col_end;
  GimpPixelRgn rgn_out;
  guchar *outrow;
  gdouble value, rd, gr, bl, al;
  gint vs, y, x, k;
  gint update_step;

  image_ID = VMAP_FUNC_ARG (data)->image_ID;
  x_off = VMAP_FUNC_ARG (data)->x_off;
  y_off = VMAP_FUNC_ARG (data)->y_off;
  name = VMAP_FUNC_ARG (data)->name;
  col_start = VMAP_FUNC_ARG (data)->colour_start;
  col_end = VMAP_FUNC_ARG (data)->colour_end;

  w = vmap->width;
  h = vmap->height;
  buffer = vmap->buffer;
  depth = vmap->depth;

  gimp_progress_init (_("Drawing seam map..."));
  update_step = MAX ((h - 1) / 20, 1);

  seam_layer_ID =
    gimp_layer_new (image_ID, name, w, h, GIMP_RGBA_IMAGE, 100,
                    GIMP_NORMAL_MODE);
  gimp_drawable_fill (seam_layer_ID, GIMP_TRANSPARENT_FILL);
  gimp_image_add_layer (image_ID, seam_layer_ID, -1);
  gimp_layer_translate (seam_layer_ID, x_off, y_off);
  drawable = gimp_drawable_get (seam_layer_ID);

  bpp = 4;

  gimp_pixel_rgn_init (&rgn_out, drawable, 0, 0, w, h, TRUE, TRUE);

  CATCH_MEM (outrow = g_try_new (guchar, w * bpp));

  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
        {
          vs = buffer[y * w + x];
          if (vs == 0)
            {
              for (k = 0; k < bpp; k++)
                {
                  outrow[x * bpp + k] = 0;
                }
            }
          else
            {
              value = (double) (depth + 1 - vs) / (depth + 1);
              rd = value * col_start.r + (1 - value) * col_end.r;
              gr = value * col_start.g + (1 - value) * col_end.g;
              bl = value * col_start.b + (1 - value) * col_end.b;
              al = 0.5 * (1 + value);
              outrow[x * bpp] = 255 * rd;
              outrow[x * bpp + 1] = 255 * gr;
              outrow[x * bpp + 2] = 255 * bl;
              outrow[x * bpp + 3] = 255 * al;
            }
        }
      gimp_pixel_rgn_set_row (&rgn_out, outrow, 0, y, w);
      if (y % update_step == 0)
        {
          gimp_progress_update ((gdouble) y / (h - 1));
        }
    }

  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (seam_layer_ID, TRUE);
  gimp_drawable_update (seam_layer_ID, 0, 0, w, h);
  gimp_drawable_set_visible (seam_layer_ID, FALSE);

  return LQR_OK;
}

LqrRetVal
write_all_vmaps (LqrVMapList * list, gint32 image_ID, gchar * orig_name,
                 gint x_off, gint y_off, GimpRGB col_start, GimpRGB col_end)
{
  gchar name[LQR_MAX_NAME_LENGTH];
  VMapFuncArg data;

  /* The name of the layer with the seams map */
  /* (here "%s" represents the selected layer's name) */
  snprintf (name, LQR_MAX_NAME_LENGTH, _("%s seam map"), orig_name);

  data.image_ID = image_ID;
  data.name = name;
  data.x_off = x_off;
  data.y_off = y_off;
  data.colour_start = col_start;
  data.colour_end = col_end;

  return lqr_vmap_list_foreach (list, write_vmap_to_layer,
                                (gpointer) (&data));
}


/* plot the energy (at current size / visibility) to a file
 * (greyscale) */
LqrRetVal
lqr_external_write_energy (LqrCarver * r /*, pngwriter& output */ )
{
  int x, y;
  //double e;

  if (!r->transposed)
    {
      /* external_resize(r->w, r->h); */
    }
  else
    {
      /* external_resize(r->h, r->w); */
    }

  lqr_carver_scan_reset (r);
  for (y = 1; y <= r->h; y++)
    {
      for (x = 1; x <= r->w; x++)
        {
          //e = r->en[r->c->now];
          if (!r->transposed)
            {
              /* external_write(x, y, e, e, e); */
            }
          else
            {
              /* external_write(y, x, e, e, e); */
            }
          //lqr_carver_read_next (r);
        }
    }

  return LQR_OK;
}
