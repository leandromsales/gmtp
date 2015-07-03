# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstgmtp.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstgmtp_la_SOURCES = gstgmtp.c gstgmtp.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgmtp_la_CFLAGS = $(GST_CFLAGS)
libgstgmtp_la_LIBADD = $(GST_LIBS)
libgstgmtp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgmtp_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstgmtp.h
