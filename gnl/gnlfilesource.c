/* Gnonlin
 * Copyright (C) <2005-2008> Edward Hervey <bilboed@bilboed.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gnl.h"

/**
 * SECTION:element-gnlfilesource
 * @short_description: GNonLin File Source
 *
 * GnlFileSource is a #GnlURISource which reads and decodes the contents
 * of a given file. The data in the file is decoded using any available
 * GStreamer plugins.
 */

static GstStaticPadTemplate gnl_filesource_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gnlfilesource);
#define GST_CAT_DEFAULT gnlfilesource


GST_BOILERPLATE (GnlFileSource, gnl_filesource, GnlURISource,
    GNL_TYPE_URI_SOURCE);

enum
{
  ARG_0,
  ARG_LOCATION,
};

static void
gnl_filesource_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void
gnl_filesource_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gnl_filesource_base_init (gpointer g_class)
{
  GstElementClass *gstclass = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstclass, "GNonLin File Source",
      "Filter/Editor",
      "High-level File Source element", "Edward Hervey <bilboed@bilboed.com>");
}

static void
gnl_filesource_class_init (GnlFileSourceClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GNL_TYPE_URI_SOURCE);

  GST_DEBUG_CATEGORY_INIT (gnlfilesource, "gnlfilesource",
      GST_DEBUG_FG_BLUE | GST_DEBUG_BOLD, "GNonLin File Source Element");

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gnl_filesource_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gnl_filesource_get_property);

  g_object_class_install_property (gobject_class, ARG_LOCATION,
      g_param_spec_string ("location", "Location",
          "Location of the file to use", NULL, G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gnl_filesource_src_template));
}

static void
gnl_filesource_init (GnlFileSource * filesource,
    GnlFileSourceClass * klass G_GNUC_UNUSED)
{
}

static void
gnl_filesource_set_location (GnlFileSource * fs, const gchar * location)
{
  gchar *tmp;

  GST_DEBUG_OBJECT (fs, "location: '%s'", location);

  if (g_ascii_strncasecmp (location, "file://", 7))
    tmp = g_strdup_printf ("file://%s", location);
  else
    tmp = g_strdup (location);
  GST_DEBUG ("%s", tmp);
  g_object_set (fs, "uri", tmp, NULL);
  g_free (tmp);
}

static void
gnl_filesource_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GnlFileSource *fs = (GnlFileSource *) object;

  switch (prop_id) {
    case ARG_LOCATION:
      /* proxy to gnomevfssrc */
      gnl_filesource_set_location (fs, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnl_filesource_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GnlFileSource *fs = (GnlFileSource *) object;

  switch (prop_id) {
    case ARG_LOCATION:
    {
      const gchar *uri = NULL;;

      g_object_get (fs, "uri", &uri, NULL);
      if (uri != NULL && g_str_has_prefix (uri, "file://"))
        g_value_set_string (value, uri + 7);
      else
        g_value_set_string (value, NULL);
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}
