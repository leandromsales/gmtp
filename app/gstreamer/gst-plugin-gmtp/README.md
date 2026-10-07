1. What is GMTP?
-----------------

TBD

2. What is gst-plugin-gmtp
--------------------------

TBD

3. Local GStreamer 1.29.2
-------------------------

Plugin work uses the development tree in `gmtp/tools/gstreamer` (tag 1.29.2).
That tree is configured with Meson and installed into
`gmtp/tools/gstreamer/prefix`. Nothing from this build is installed into
`/usr`, `/usr/local`, or Homebrew.

The prefix contains the core, gst-plugins-base, gst-plugins-good,
gst-plugins-bad, gst-plugins-ugly, gst-libav, gst-python, gst-rtsp-server,
gst-editing-services, and gst-devtools, plus GObject introspection data.
Qt5, Qt6, and the Vulkan plugin are not part of this build.

`gst-libav` in that tree has a local change so it compiles against Homebrew
FFmpeg, which no longer declares `AV_CODEC_ID_V308`, `AV_CODEC_ID_V408`, and
`AV_CODEC_ID_V410`.

### 3.1 Environment

Set `GMTP_ROOT` to the `gmtp/` checkout, then export the variables below in
every shell that runs `gst-inspect-1.0`, `gst-launch-1.0`, compiles a plugin,
or builds a C, C++, Python, or Go application.

```sh
# From the gmtp/ directory
export GMTP_ROOT="$(pwd)"
export GST_PREFIX="$GMTP_ROOT/tools/gstreamer/prefix"

export PATH="$GST_PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$GST_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export GST_PLUGIN_SYSTEM_PATH="$GST_PREFIX/lib/gstreamer-1.0"
export GST_PLUGIN_PATH="$GMTP_ROOT/app/gstreamer/gst-plugin-gmtp/src/.libs${GST_PLUGIN_PATH:+:$GST_PLUGIN_PATH}"
export GST_REGISTRY="$GST_PREFIX/gstreamer-1.0.registry"
export GI_TYPELIB_PATH="$GST_PREFIX/lib/girepository-1.0${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"

# macOS. Homebrew GLib is required so typelibs can dlopen libgobject.
export DYLD_LIBRARY_PATH="$GST_PREFIX/lib:/opt/homebrew/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

# Linux
# export LD_LIBRARY_PATH="$GST_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# gst-python overrides. This checkout was built for Homebrew Python 3.13.
export PYTHONPATH="$GST_PREFIX/opt/homebrew/lib/python3.13/site-packages${PYTHONPATH:+:$PYTHONPATH}"
```

If `Gst.py` is not at that `PYTHONPATH`, locate it and use its `site-packages`
directory:

```sh
find "$GST_PREFIX" -name Gst.py
```

The first `gst-inspect-1.0` builds the plugin registry and can take about a
minute. Later runs use `$GST_REGISTRY`.

Check the tools:

```sh
gst-inspect-1.0 --version
gst-inspect-1.0 coreelements
pkg-config --modversion gstreamer-1.0 gstreamer-base-1.0
```

`gst-inspect-1.0 --version` must print `1.29.2`. `which gst-inspect-1.0` must
be `$GST_PREFIX/bin/gst-inspect-1.0`.

### 3.2 C and C++

Headers and shared libraries come from `pkg-config`. A C or C++ program links
the same way:

```sh
cc -o app app.c $(pkg-config --cflags --libs gstreamer-1.0)
c++ -o app app.cpp $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-video-1.0)
```

Use `gstreamer-base-1.0` as well when the code includes `gst/base`.
`pkg-config --libs` records the absolute library path inside
`$GST_PREFIX/lib`, so the resulting binary keeps using this build.

### 3.3 Python

Use the Python that this build was configured for, Homebrew Python 3.13.
PyGObject (`gi`) must be installed for that interpreter.

```sh
/opt/homebrew/bin/python3.13 -c 'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst; Gst.init(None); print(Gst.version_string())'
```

### 3.4 Go

Go applications use cgo and the same `pkg-config` files. `CGO_ENABLED=1` and
`PKG_CONFIG_PATH` from section 3.1 are required. A program can call the C API
directly:

```go
package main

/*
#cgo pkg-config: gstreamer-1.0
#include <gst/gst.h>
*/
import "C"
import "fmt"

func main() {
	C.gst_init(nil, nil)
	fmt.Println(C.GoString(C.gst_version_string()))
}
```

Higher-level modules such as `github.com/go-gst/go-gst` use the same
`PKG_CONFIG_PATH`.

### 3.5 Compiling gst-plugin-gmtp

From this directory, with the environment in section 3.1:

```sh
./autogen.sh
make
gst-inspect-1.0 gmtp
```

`gst-inspect-1.0 gmtp` lists `gmtpclientsrc`, `gmtpclientsink`,
`gmtpserversrc`, and `gmtpserversink`. The plugin file is
`src/.libs/libgstgmtp.so`. `$GST_PLUGIN_PATH` already points there.

### 3.6 Pipelines

GMTP sockets need a kernel that implements `SOCK_GMTP`. These pipelines load
the plugin. Playback elements such as `osxaudiosink` or `autoaudiosink` depend
on the host.

```sh
gst-launch-1.0 -v filesrc location=~/audiofile.mp3 ! mpegaudioparse ! gmtpserversink port=12345
gst-launch-1.0 -v gmtpclientsrc host=127.0.0.1 port=12345 ! decodebin ! audioconvert ! autoaudiosink
```

4. Porting to GStreamer 1.0
---------------------------

See http://cgit.freedesktop.org/gstreamer/gstreamer/tree/docs/random/porting-to-1.0.txt

HOW TO USE THE TEMPLATE PLUGIN
------------------------------

To use it, either make a copy for yourself and rename the parts or use the
make_element script in tools. To create sources for "myfilter" based on the
"gsttransform" template run:

cd src;
../tools/make_element myfilter gsttransform

This will create gstmyfilter.c and gstmyfilter.h. Open them in an editor and
start editing. There are several occurances of the string "template", update
those with real values. The plugin will be called 'myfilter' and it will have
one element called 'myfilter' too. Also look for "FIXME:" markers that point you
to places where you need to edit the code.

You still need to adjust the Makefile.am.
