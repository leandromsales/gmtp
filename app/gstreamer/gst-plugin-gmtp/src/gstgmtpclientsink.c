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

/**
 * SECTION:element-gmtpclientsink
 * @see_also: gmtpserversrc, gmtpclientsrc, gmtpserversink
 *
 * This element connect to a GMTP server and send data to it.
 * <ulink url="http://www.linuxfoundation.org/en/Net:GMTP">GMTP</ulink> (Datagram
 * Congestion Control Protocol) is a Transport Layer protocol like
 * TCP and UDP.
 *
 * <refsect2>
 * <title>Example pipeline</title>
 * <para>
 * |[
 * gst-launch -v filesrc location=music.mp3 ! mp3parse ! gmtpclientsink host=localhost port=9011 ccid=2
 * ]| Client
 * |[
 * gst-launch -v gmtpserversrc port=9011 ccid=2 ! decodebin ! alsasink
 * ]| Server
 *
 * This example pipeline will send a MP3 stream to the server using GMTP.
 * The server will decode the MP3 and play it.
 * Run the server pipeline first than the client pipeline.
 * </para>
 * </refsect2>
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgmtpclientsink.h"
#include "gstgmtp.h"

/* signals */
enum
{
  SIGNAL_CONNECTED,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_PORT,
  PROP_HOST,
  PROP_SOCK_FD,
/*  PROP_CCID,*/
  PROP_CLOSE_FD
};

static gboolean gst_gmtp_client_sink_stop (GstBaseSink * bsink);
static gboolean gst_gmtp_client_sink_start (GstBaseSink * bsink);
static GstFlowReturn gst_gmtp_client_sink_render (GstBaseSink * bsink,
    GstBuffer * buf);
static void gst_gmtp_client_sink_finalize (GObject * gobject);

GST_DEBUG_CATEGORY_STATIC (gmtpclientsink_debug);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define gst_gmtp_client_sink_parent_class parent_class
G_DEFINE_TYPE (GstGMTPClientSink, gst_gmtp_client_sink, GST_TYPE_BASE_SINK);

static guint gst_gmtp_client_sink_signals[LAST_SIGNAL] = { 0 };

/*
 * Write buffer to client socket.
 *
 * @return GST_FLOW_OK if the send operation was successful, GST_FLOW_ERROR otherwise.
 */
static GstFlowReturn
gst_gmtp_client_sink_render (GstBaseSink * bsink, GstBuffer * buf)
{
  GstGMTPClientSink *sink = GST_GMTP_CLIENT_SINK (bsink);

  return gst_gmtp_send_buffer (GST_ELEMENT (sink), buf, sink->sock_fd,
      sink->pksize);
}

/*
 * Set the value of a property for the client sink.
 */
static void
gst_gmtp_client_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGMTPClientSink *sink = GST_GMTP_CLIENT_SINK (object);

  switch (prop_id) {
    case PROP_PORT:
      sink->port = g_value_get_int (value);
      break;
    case PROP_SOCK_FD:
      sink->sock_fd = g_value_get_int (value);
      break;
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (sink->host);
      sink->host = g_strdup (g_value_get_string (value));
      break;
    case PROP_CLOSE_FD:
      sink->closed = g_value_get_boolean (value);
      break;
   /* case PROP_CCID:
      sink->ccid = g_value_get_int (value);
      break;*/
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
 * Get a given property value for the client sink.
 */
static void
gst_gmtp_client_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGMTPClientSink *sink = GST_GMTP_CLIENT_SINK (object);

  switch (prop_id) {
    case PROP_PORT:
      g_value_set_int (value, sink->port);
      break;
    case PROP_SOCK_FD:
      g_value_set_int (value, sink->sock_fd);
      break;
    case PROP_HOST:
      g_value_set_string (value, sink->host);
      break;
    case PROP_CLOSE_FD:
      g_value_set_boolean (value, sink->closed);
      break;
   /* case PROP_CCID:
      g_value_set_int (value, sink->ccid);
      break;*/
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gmtp_client_sink_finalize (GObject * gobject)
{
  GstGMTPClientSink *this = GST_GMTP_CLIENT_SINK (gobject);

  g_free (this->host);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/*
 * Starts the element. If the sockfd property was not the default, this method
 * will create a new socket and connect to the server.
 *
 * @param bsink - the element
 * @return TRUE if the send operation was successful, FALSE otherwise.
 */
static gboolean
gst_gmtp_client_sink_start (GstBaseSink * bsink)
{
  GstGMTPClientSink *sink = GST_GMTP_CLIENT_SINK (bsink);

  if (sink->sock_fd == GMTP_DEFAULT_SOCK_FD) {
    gchar *ip = NULL;

    /* look up name if we need to */
    if (!(ip = gst_gmtp_host_to_ip (GST_ELEMENT (sink), sink->host))) {
      GST_ERROR_OBJECT (sink, "cannot resolve hostname");
      gst_gmtp_client_sink_stop (GST_BASE_SINK (sink));
      return FALSE;
    }

    /* name the server socket */
    memset (&sink->server_sin, 0, sizeof (sink->server_sin));
    sink->server_sin.sin_family = AF_INET;      /* network socket */
    sink->server_sin.sin_port = htons (sink->port);     /* on port */
    sink->server_sin.sin_addr.s_addr = inet_addr (ip);  /* on host ip */
    g_free (ip);

    /* create socket */
    if ((sink->sock_fd = gst_gmtp_create_new_socket (GST_ELEMENT (sink))) < 0) {
      return FALSE;
    }
/*
    if (!gst_gmtp_set_ccid (GST_ELEMENT (sink), sink->sock_fd, sink->ccid)) {
      gst_gmtp_client_sink_stop (GST_BASE_SINK (sink));
      return FALSE;
    }*/

    if (!gst_gmtp_connect_to_server (GST_ELEMENT (sink), sink->server_sin,
            sink->sock_fd)) {
      gst_gmtp_client_sink_stop (GST_BASE_SINK (sink));
      return FALSE;
    }

    /* the socket is connected */
    g_signal_emit (sink, gst_gmtp_client_sink_signals[SIGNAL_CONNECTED], 0,
        sink->sock_fd);
  }

  sink->pksize =
      gst_gmtp_get_max_packet_size (GST_ELEMENT (sink), sink->sock_fd);

  return TRUE;
}

static void
gst_gmtp_client_sink_init (GstGMTPClientSink * this)
{
  this->port = GMTP_DEFAULT_PORT;
  this->host = g_strdup (GMTP_DEFAULT_HOST);
  this->sock_fd = GMTP_DEFAULT_SOCK_FD;
  this->closed = GMTP_DEFAULT_CLOSED;
 /* this->ccid = GMTP_DEFAULT_CCID;*/
}

static gboolean
gst_gmtp_client_sink_stop (GstBaseSink * bsink)
{
  GstGMTPClientSink *sink;

  sink = GST_GMTP_CLIENT_SINK (bsink);

  if (sink->sock_fd != GMTP_DEFAULT_SOCK_FD && sink->closed) {
    gst_gmtp_socket_close (GST_ELEMENT (sink), &(sink->sock_fd));
  }

  return TRUE;
}

static GstCaps *
gst_gmtp_client_sink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstGMTPClientSink *sink;
  GstCaps *caps = NULL;

  sink = GST_GMTP_CLIENT_SINK (bsink);

  caps = (filter ? gst_caps_ref (filter) : gst_caps_new_any ());

  GST_DEBUG_OBJECT (sink, "returning caps %" GST_PTR_FORMAT, caps);
  g_assert (GST_IS_CAPS (caps));
  return caps;
}

static gboolean
gst_gmtp_client_sink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstGMTPClientSink *sink;
  sink = GST_GMTP_CLIENT_SINK (bsink);
  GST_INFO("CHAMOUT SET CAPS DE CLIENT SINK");
  if ((caps != NULL) && (!gst_caps_is_equal (caps, GST_CAPS_ANY))) {
    if (!gst_pad_set_caps(bsink->sinkpad, caps)) {
       GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, (NULL),
          ("Error setting caps."));
      return FALSE;
    }
  }
  return TRUE;
}

/*
 * Define the gst class, callbacks, etc.
 */
static void
gst_gmtp_client_sink_class_init (GstGMTPClientSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gmtp_client_sink_set_property;
  gobject_class->get_property = gst_gmtp_client_sink_get_property;
  gobject_class->finalize = gst_gmtp_client_sink_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_static_metadata (gstelement_class, "GMTP client sink",
      "Sink/Network",
      "Send data as a client over the network via GMTP",
      "E-Phone Team at Federal University of Campina Grande <leandroal@gmail.com>");

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PORT,
      g_param_spec_int ("port", "Port",
          "The port to send the packets to", 0, G_MAXUINT16,
          GMTP_DEFAULT_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host",
          "The host IP address to send packets to", GMTP_DEFAULT_HOST,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOCK_FD,
      g_param_spec_int ("sockfd", "Socket fd",
          "The socket file descriptor", -1, G_MAXINT,
          GMTP_DEFAULT_SOCK_FD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CLOSE_FD,
      g_param_spec_boolean ("close-socket", "Close",
          "Close socket at end of stream",
          GMTP_DEFAULT_CLOSED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /* g_object_class_install_property (gobject_class, PROP_CCID,
      g_param_spec_int ("ccid", "CCID",
          "The Congestion Control IDentified to be used", 2, G_MAXINT,
          GMTP_DEFAULT_CCID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));*/

  /* signals */
  /**
   * GstDccpClientSink::connected:
   * @sink: the gstgmtpclientsink element that emitted this signal
   * @fd: the connected socket file descriptor
   *
   * Sign that the element has connected, return the fd of the socket.
   */
  gst_gmtp_client_sink_signals[SIGNAL_CONNECTED] =
      g_signal_new ("connected", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
      G_STRUCT_OFFSET (GstGMTPClientSinkClass, connected), NULL, NULL,
      g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_INT);

  gstbasesink_class->start = gst_gmtp_client_sink_start;
  gstbasesink_class->stop = gst_gmtp_client_sink_stop;
  gstbasesink_class->render = gst_gmtp_client_sink_render;
  //gstbasesink_class->set_caps = gst_gmtp_client_sink_setcaps;
  gstbasesink_class->get_caps = gst_gmtp_client_sink_getcaps;

  GST_DEBUG_CATEGORY_INIT (gmtpclientsink_debug, "gmtpclientsink", 0,
      "GMTP Client Sink");
}
