NULL=

INCLUDES = 						\
	-DG_LOG_DOMAIN=\"memo-file-conduit\" 		\
	-I$(includedir) 				\
	-I$(top_srcdir) 				\
	$(GNOME_PILOT_CONDUITS_CFLAGS)

memo_file_conduitsdir=$(libdir)/gnome-pilot/conduits

memo_file_conduits_LTLIBRARIES = \
	libmemo_file_conduit.la

libmemo_file_conduit_la_LDFLAGS = -module -avoid-version

libmemo_file_conduit_la_SOURCES = 	\
	memo_file_conduit.c 		\
	memo_file_conduit.h		\
	$(NULL)

libmemo_file_conduit_la_LIBADD = 	\
	$(GNOME_PILOT_CONDUITS_LIBS)

@GPC_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = memofile.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 				\
	memofile.conduit.in		\
	$(NULL)
