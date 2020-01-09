NULL=

INCLUDES = 						\
	-I$(includedir) 				\
	-I@top_srcdir@/mal-conduit/mal/common 		\
	-I@top_srcdir@/mal-conduit/mal/client/common 	\
	-DG_LOG_DOMAIN=\"MALconduit\"			\
	$(GNOME_PILOT_CONDUITS_CFLAGS)			\
	$(NULL)

mal_libdir=$(libdir)/gnome-pilot/conduits

noinst_LTLIBRARIES = 		\
	libmal_client_unix.la	\
	$(NULL)


# put this in sources for SunOS.
#	ap_snprintf.c

libmal_client_unix_la_SOURCES = \
	MAL31UserConfig.c	\
	MAL31ServerConfig.c	\
	MAL31DBConfig.c		\
	malsync.c		\
	malsync.h		\
	MAL31UserConfig.h	\
	MAL31ServerConfig.h	\
	MAL31DBConfig.h		\
	getopt.h		\
	$(NULL)

install-data-local:
	$(mkinstalldirs) $(Conduitsdir)

EXTRA_DIST=README_malsync
