INCLUDES = 			\
	-I$(includedir) 	\
	-I$(top_srcdir) 	\
	$(GNOME_PILOT_CONDUITS_CFLAGS)

time_conduitsdir=$(libdir)/gnome-pilot/conduits

time_conduits_LTLIBRARIES = \
	libtime_conduit.la

libtime_conduit_la_LDFLAGS = -module -avoid-version

libtime_conduit_la_SOURCES = 	\
	time-conduit.c 		\
	time-conduit.h

libtime_conduit_la_LIBADD = 	\
	$(GNOME_PILOT_CONDUITS_LIBS)

@GPC_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = time.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 			\
	time.conduit.in
