%.conduit: %.conduit.in Makefile
	sed -e 's|\@LIBDIR\@|$(libdir)|' 	\
	-e 's|\@DATADIR\@|$(datadir)|' 	$< > $@
