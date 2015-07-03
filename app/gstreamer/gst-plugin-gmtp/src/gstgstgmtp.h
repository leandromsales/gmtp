/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2015 Leandro <<user@hostname.org>>
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
 
#ifndef __GST_GSTGMTP_H__
#define __GST_GSTGMTP_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_GSTGMTP \
  (gst_gstgmtp_get_type())
#define GST_GSTGMTP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GSTGMTP,Gstgstgmtp))
#define GST_GSTGMTP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GSTGMTP,GstgstgmtpClass))
#define GST_IS_GSTGMTP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GSTGMTP))
#define GST_IS_GSTGMTP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GSTGMTP))

typedef struct _Gstgstgmtp      Gstgstgmtp;
typedef struct _GstgstgmtpClass GstgstgmtpClass;

struct _Gstgstgmtp {
  GstBaseTransform element;

  gboolean silent;
};

struct _GstgstgmtpClass {
  GstBaseTransformClass parent_class;
};

GType gst_gstgmtp_get_type (void);

G_END_DECLS

#endif /* __GST_GSTGMTP_H__ */
