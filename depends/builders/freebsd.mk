build_openbsd_SHA256SUM = cksum -a sha256
build_openbsd_DOWNLOAD = curl --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) -L -o
build_openbsd_TAR = gtar
build_openbsd_SED = gsed

#freebsd host on freebsd builder. overrides host preferences.
x86_64_freebsd_CC=cc
x86_64_freebsd_CXX=c++
x86_64_freebsd_AR=ar
x86_64_freebsd_RANLIB=ranlib
x86_64_freebsd_STRIP=strip
x86_64_freebsd_NM=nm

i386_freebsd_CC=cc
i386_freebsd_CXX=c++
i386_freebsd_AR=ar
i386_freebsd_RANLIB=ranlib
i386_freebsd_STRIP=strip
i386_freebsd_NM=nm

freebsd_native_toolchain=
