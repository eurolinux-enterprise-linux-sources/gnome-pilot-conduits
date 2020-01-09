INCLUDES = 			\
	-I$(includedir) 	\
	-I$(top_srcdir) 	\
	$(GNOME_PILOT_CONDUITS_CFLAGS)

email_conduitsdir=$(libdir)/gnome-pilot/conduits

email_conduits_LTLIBRARIES = \
	libemail_conduit.la

libemail_conduit_la_LDFLAGS = -module -avoid-version

libemail_conduit_la_SOURCES = 	\
	email_conduit.c 	\
	email_conduit.h 	\
	parsedate.y

libemail_conduit_la_LIBADD = 			\
	$(GNOME_PILOT_CONDUITS_LIBS)

@GPC_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = sendmail.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 			\
	 sendmail.conduit.in
