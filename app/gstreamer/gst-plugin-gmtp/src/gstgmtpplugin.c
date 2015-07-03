/* GStreamer
 * Copyright (C) <2007> Leandro Melo de Sales <leandroal@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgmtpclientsrc.h"
#include "gstgmtpserversink.h"
#include "gstgmtpclientsink.h"
#include "gstgmtpserversrc.h"

GST_DEBUG_CATEGORY (gmtp_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "gmtpclientsrc", GST_RANK_NONE,
          GST_TYPE_GMTP_CLIENT_SRC))
    return FALSE;

  if (!gst_element_register (plugin, "gmtpserversink", GST_RANK_NONE,
          GST_TYPE_GMTP_SERVER_SINK))
    return FALSE;

  if (!gst_element_register (plugin, "gmtpclientsink", GST_RANK_NONE,
          GST_TYPE_GMTP_CLIENT_SINK))
    return FALSE;

  if (!gst_element_register (plugin, "gmtpserversrc", GST_RANK_NONE,
          GST_TYPE_GMTP_SERVER_SRC))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gmtp_debug, "gmtp", 0, "GMTP calls");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gmtp,
    "transfer data over the network via GMTP.",
    plugin_init, VERSION, GST_LICENSE, "GMTP",
    "http://garage.maemo.org/projects/ephone")
