lib_LTLIBRARIES = libicalmapi.la

AM_CPPFLAGS = $(DEBUGFLAGS) $(BOOST_CPPFLAGS) -DLINUX $(ICAL_CFLAGS) \
	-I$(top_srcdir)/mapi4linux/include -I$(top_srcdir)/provider/common  -I${top_srcdir}/common \
	-I$(top_srcdir)/libfreebusy

libicalmapi_la_LDFLAGS = -version-info $(ECCLIENT_LIBRARY_VERSION)

libicalmapi_la_LIBADD = \
	${top_builddir}/libfreebusy/libfreebusy.la \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la \
	${top_builddir}/common/libcommon_util.la \
	$(ICAL_LIBS)

libicalmapidir = $(includedir)/icalmapi
libicalmapi_HEADERS = icalitem.h nameids.h icaluid.h \
	MAPIToICal.h ICalToMAPI.h \
	vtimezone.h valarm.h vconverter.h vevent.h vtodo.h \
	icalrecurrence.h icalmapi.h vfreebusy.h

libicalmapi_la_SOURCES = icalinit.cpp \
	nameids.cpp \
	icaluid.cpp \
	MAPIToICal.cpp \
	ICalToMAPI.cpp \
	vtimezone.cpp \
	valarm.cpp \
	vconverter.cpp \
	vevent.cpp \
	vtodo.cpp \
	icalrecurrence.cpp \
	vfreebusy.cpp


check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
