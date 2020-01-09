NULL=

INCLUDES = -I$(includedir) $(GNOME_INCLUDEDIR) $(PISOCK_INCLUDEDIR) 	\
	-I@top_srcdir@/mal-conduit/mal/common 				\
	-I@top_srcdir@/mal-conduit/mal/client/common 			\
	-DG_LOG_DOMAIN=\"MALconduit\"					\
	$(NULL)

mal_libdir=$(libdir)/gnome-pilot/conduits

noinst_LTLIBRARIES = \
	libmal_client_common.la

libmal_client_common_la_SOURCES = 	\
	AGSyncProcessor.c		\
	AGSyncProcessor.h		\
	AGCommandProcessor.c		\
	AGCommandProcessor.h		\
	AGClientProcessor.c		\
	AGClientProcessor.h		\
	AGTimeoutMessageBox.h		\
	AGSyncProcessor.h		\
	AGSyncCommon.h			\
	AGShlapi.h			\
	AGProxyDebug.h			\
	AGPasswordPrompt.h		\
	AGMobileLinkSettings.h		\
	AGLinkMessageBox.h		\
	AGLinkMBResource.h		\
	$(NULL)


install-data-local:
	$(mkinstalldirs) $(Conduitsdir)

