FREEBSD_VERSION=9.3
FREEBSD_SYSROOT=$(build_prefix)/sysroot

ifeq ($(findstring 9.,$(FREEBSD_VERSION)),9.)
FREEBSD_STDLIB=libstdc++
else
FREEBSD_STDLIB=libc++
endif

ifeq ($(findstring x86_64,$(host_arch)),x86_64)
FREEBSD_ARCH=amd64
else
ifeq ($(findstring 86,$(host_arch)),86)
FREEBSD_ARCH=i386
endif
endif

freebsd_CFLAGS=-pipe
freebsd_CXXFLAGS=$(freebsd_CFLAGS)

freebsd_release_CFLAGS=-O2
freebsd_release_CXXFLAGS=$(freebsd_release_CFLAGS)

freebsd_debug_CFLAGS=-O1
freebsd_debug_CXXFLAGS=$(freebsd_debug_CFLAGS)

freebsd_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
x86_64_freebsd_CC=$(build_prefix)/bin/clang -target $(host) --sysroot $(FREEBSD_SYSROOT)
x86_64_freebsd_CXX=$(build_prefix)/bin/clang++ -target $(host) -stdlib=$(FREEBSD_STDLIB) --sysroot $(FREEBSD_SYSROOT)
x86_64_freebsd_AR=ar
x86_64_freebsd_RANLIB=ranlib
x86_64_freebsd_STRIP=strip
x86_64_freebsd_NM=nm

i386_freebsd_CC=$(build_prefix)/bin/clang -target $(host) --sysroot $(FREEBSD_SYSROOT)
i386_freebsd_CXX=$(build_prefix)/bin/clang++ -target $(host) -stdlib=$(FREEBSD_STDLIB) --sysroot $(FREEBSD_SYSROOT)
i386_freebsd_AR=ar
i386_freebsd_RANLIB=ranlib
i386_freebsd_STRIP=strip
i386_freebsd_NM=nm

freebsd_native_toolchain=native_freebsd
