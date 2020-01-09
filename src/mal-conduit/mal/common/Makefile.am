INCLUDES = -I$(includedir) $(GNOME_INCLUDEDIR) $(PISOCK_INCLUDEDIR) 	\
	-I@top_srcdir@/mal-conduit/mal/common 				\
	-I@top_srcdir@/mal-conduit/mal/client/common 			\
	-DG_LOG_DOMAIN=\"MALconduit\"

mal_libdir=$(libdir)/gnome-pilot/conduits

noinst_LTLIBRARIES = \
	libmal_common.la

libmal_common_la_SOURCES = 	\
	AGNet.c 		\
	AGBase64.c 		\
	AGProxy.c 		\
	AGArray.c 		\
	AGDBConfig.c 		\
	AGProtectedMem.c 	\
	AGLocationConfig.c 	\
	AGUtilUnix.c 		\
	AGDeviceInfo.c 		\
	AGProtocol.c 		\
	AGPalmProtocol.c 	\
	AGWriter.c 		\
	AGBufferReader.c 	\
	AGHashTable.c 		\
	AGReader.c 		\
	AGBufferWriter.c 	\
	AGMsg.c 		\
	AGRecord.c 		\
	AGCollection.c 		\
	AGDigest.c 		\
	AGMD5.c 		\
	AGSynchronize.c		\
	AGUserConfig.c 		\
	AGServerConfig.c 	\
	AGBufferedNet.c		\
	AGCEProtocol.c		\
	AGWriter.h		\
	AGUtilWin.h		\
	AGUtilUnix.h		\
	AGUtilPalmOS.h		\
	AGUtil.h		\
	AGUserConfig.h		\
	AGTypes.h		\
	AGSynchronize.h		\
	AGServerConfig.h	\
	AGRecord.h		\
	AGReader.h		\
	AGProxy.h		\
	AGProtocol.h		\
	AGProtectedMem.h	\
	AGPalmProtocol.h	\
	AGNet.h			\
	AGMsg.h			\
	AGMD5.h			\
	AGLocationConfig.h	\
	AGHashTable.h		\
	AGDigest.h		\
	AGDeviceInfo.h		\
	AGDesktopInfoPalm.h	\
	AGDBConfig.h		\
	AGCollection.h		\
	AGBufferWriter.h	\
	AGBufferReader.h	\
	AGBase64.h		\
	AGArray.h		\
	AGBufferedNet.h		\
	AGCEProtocol.h		\
	AGProxySearch.h

install-data-local:
	$(mkinstalldirs) $(Conduitsdir)

