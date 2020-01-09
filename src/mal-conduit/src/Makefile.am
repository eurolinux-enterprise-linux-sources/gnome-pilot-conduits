NULL=
INCLUDES = 								\
	-DG_LOG_DOMAIN=\"MALconduit\" 					\
	-DCONDUIT_VERSION=\"@VERSION@\"	\
	-I$(includedir) 						\
	-I@top_srcdir@/mal-conduit/mal/common 				\
	-I@top_srcdir@/mal-conduit/mal/client/common 			\
	-I@top_srcdir@/mal-conduit/mal/client/unix 			\
	$(GNOME_PILOT_CONDUITS_CFLAGS)

mal_conduitsdir=$(libdir)/gnome-pilot/conduits

mal_conduits_LTLIBRARIES = 	\
	libmal_conduit.la	\
	$(NULL)

libmal_conduit_la_LDFLAGS = -module -avoid-version

libmal_conduit_la_SOURCES = 	\
	mal-conduit.h 		\
	mal-conduit.c		\
	$(NULL)

libmal_conduit_la_LIBADD =  					\
	../mal/common/libmal_common.la 				\
	../mal/client/unix/libmal_client_unix.la 		\
	../mal/client/common/libmal_client_common.la 		\
	$(GNOME_PILOT_CONDUITS_LIBS)

@GPC_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = mal.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 			\
	avantgo.png		\
	mal.conduit.in		\
	$(NULL)

appicondir = $(datadir)/pixmaps
appicon_DATA = avantgo.png

