INCLUDES = 			\
	-I$(includedir) 	\
	-I$(top_srcdir) 	\
	$(GNOME_PILOT_CONDUITS_CFLAGS)

expense_conduitsdir=$(libdir)/gnome-pilot/conduits

expense_conduits_LTLIBRARIES = \
	libexpense_conduit.la

libexpense_conduit_la_LDFLAGS = -module -avoid-version

libexpense_conduit_la_SOURCES = \
	expense_conduit.c 	\
	expense_conduit.h 

libexpense_conduit_la_LIBADD = 		\
	$(GNOME_PILOT_CONDUITS_LIBS)

@GPC_CONDUIT_RULE@

Conduitdir = $(datadir)/gnome-pilot/conduits/
Conduit_DATA = expense.conduit

CLEANFILES = $(Conduit_DATA)

EXTRA_DIST = 			\
	expense.conduit.in

