build_openbsd_SHA256SUM = cksum -a sha256
build_openbsd_DOWNLOAD = curl --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) -L -o
build_openbsd_TAR = gtar
build_openbsd_SED = gsed

#openbsd host on openbsd builder. overrides host preferences.
x86_64_openbsd_CC=cc
x86_64_openbsd_CXX=c++
x86_64_openbsd_AR=ar
x86_64_openbsd_RANLIB=ranlib
x86_64_openbsd_STRIP=strip
x86_64_openbsd_NM=nm

i386_openbsd_CC=cc
i386_openbsd_CXX=c++
i386_openbsd_AR=ar
i386_openbsd_RANLIB=ranlib
i386_openbsd_STRIP=strip
i386_openbsd_NM=nm
