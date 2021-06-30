lib_LTLIBRARIES = libbridge_rest.la

libbridge_rest_la_SOURCES  = bridge_rest.cpp
libbridge_rest_la_CXXFLAGS = -Wall \
									  -I$(top_srcdir)/src \
									  -I$(top_builddir) \
									  @ROBOTKERNEL_CFLAGS@ \
									  @LIBHTTPSERVER_CFLAGS@

libbridge_rest_la_LDFLAGS  = @LIBHTTPSERVER_LIBS@ -Wall -release $(PACKAGE_VERSION) -Wl,--exclude-libs,ALL

