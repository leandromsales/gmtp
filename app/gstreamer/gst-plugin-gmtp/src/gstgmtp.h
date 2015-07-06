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

#ifndef __GST_GMTP_H__
#define __GST_GMTP_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include "gstgmtp_common.h"

/* GMTP socket general options */
#define GMTP_BACKLOG	5
#ifndef SOCK_GMTP
	#define SOCK_GMTP		6
#endif

#ifndef IPPROTO_GMTP
	#define IPPROTO_GMTP	33
#endif

#ifndef SOL_GMTP
	#define SOL_GMTP		269
#endif

/* gmtp socket specific options */
#define GMTP_SOCKOPT_PACKET_SIZE        1 /* XXX deprecated, without effect */
#define GMTP_SOCKOPT_SERVICE            2
#define GMTP_SOCKOPT_CHANGE_L           3
#define GMTP_SOCKOPT_CHANGE_R           4
#define GMTP_SOCKOPT_GET_CUR_MPS        5
#define GMTP_SOCKOPT_SERVER_TIMEWAIT    6
#define GMTP_SOCKOPT_SEND_CSCOV         10
#define GMTP_SOCKOPT_RECV_CSCOV         11
#define GMTP_SOCKOPT_AVAILABLE_CCIDS    12
#define GMTP_SOCKOPT_CCID               13
#define GMTP_SOCKOPT_TX_CCID            14
#define GMTP_SOCKOPT_RX_CCID            15
#define GMTP_SOCKOPT_CCID_RX_INFO       128
#define GMTP_SOCKOPT_CCID_TX_INFO       192

/* Default parameters for the gst gmtp element property */
#define GMTP_DEFAULT_PORT		 5001
#define GMTP_DEFAULT_SOCK_FD		 -1
#define GMTP_DEFAULT_CLIENT_SOCK_FD	 -1
#define GMTP_DEFAULT_CLOSED		 TRUE
#define GMTP_DEFAULT_WAIT_CONNECTIONS	 FALSE
#define GMTP_DEFAULT_HOST		 "127.0.0.1"
#define GMTP_DEFAULT_CCID		 2

#define GMTP_DELTA			 100

gchar *gst_gmtp_host_to_ip (GstElement * element, const gchar * host);

GstFlowReturn gst_gmtp_read_buffer (GstElement * this, int socket,
				GstBuffer ** buf);

gint gst_gmtp_create_new_socket (GstElement * element);
gboolean gst_gmtp_connect_to_server (GstElement * element,
				 struct sockaddr_in server_sin,
				 int sock_fd);

gint gst_gmtp_server_wait_connections (GstElement * element, int server_sock_fd);

gboolean gst_gmtp_bind_server_socket (GstElement * element, int server_sock_fd,
					  struct sockaddr_in server_sin);

gboolean gst_gmtp_listen_server_socket (GstElement * element, int server_sock_fd);
gboolean gst_gmtp_set_ccid (GstElement * element, int sock_fd, uint8_t ccid);

gint gst_gmtp_get_max_packet_size(GstElement * element, int sock);

GstFlowReturn gst_gmtp_send_buffer (GstElement * element, GstBuffer * buffer,
					int client_sock_fd, int packet_size);

gboolean gst_gmtp_make_address_reusable (GstElement * element, int sock_fd);
void gst_gmtp_socket_close (GstElement * element, int * socket);

#endif /* __GST_GMTP_H__ */
