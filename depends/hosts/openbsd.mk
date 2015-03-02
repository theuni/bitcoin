ifeq ($(findstring x86_64,$(host_arch)),x86_64)
openbsd_ARCH=amd64
else
ifeq ($(findstring 86,$(host_arch)),86)
openbsd_ARCH=i386
endif
endif

openbsd_CFLAGS=-pipe
openbsd_CXXFLAGS=$(openbsd_CFLAGS)

openbsd_release_CFLAGS=-O2
openbsd_release_CXXFLAGS=$(openbsd_release_CFLAGS)

openbsd_debug_CFLAGS=-O1
openbsd_debug_CXXFLAGS=$(openbsd_debug_CFLAGS)

openbsd_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
x86_64_openbsd_CC=cc
x86_64_openbsd_CXX=cxx
x86_64_openbsd_AR=ar
x86_64_openbsd_RANLIB=ranlib
x86_64_openbsd_STRIP=strip
x86_64_openbsd_NM=nm

i386_openbsd_CC=cc
i386_openbsd_CXX=cxx
i386_openbsd_AR=ar
i386_openbsd_RANLIB=ranlib
i386_openbsd_STRIP=strip
i386_openbsd_NM=nm
