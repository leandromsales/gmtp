# plugindir is set in configure

plugin_LTLIBRARIES = libgstgmtp.la

# sources used to compile this plug-in
libgstgmtp_la_SOURCES = gstgmtpplugin.c \
			gstgmtp.c \
			gstgmtpserversink.c \
			gstgmtpserversrc.c \
			gstgmtpclientsink.c \
			gstgmtpclientsrc.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgmtp_la_CFLAGS = $(GST_CFLAGS)
libgstgmtp_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(GMTP_LIBS) $(WINSOCK2_LIBS)
libgstgmtp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
#libgstgmtp_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)
libgstgmtp_la_LIBTOOLFLAGS = --tag=disable-static


# headers we need but don't want installed
noinst_HEADERS = gstgmtp.h \
                 gstgmtpserversink.h \
                 gstgmtpserversrc.h \
                 gstgmtpclientsink.h \
                 gstgmtpclientsrc.h \
                 gstgmtp_common.h
