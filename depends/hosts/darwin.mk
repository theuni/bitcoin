OSX_MIN_VERSION=10.6
OSX_SDK_VERSION=10.7
OSX_SDK=$(SDK_PATH)/MacOSX$(OSX_SDK_VERSION).sdk
darwin_CC=clang -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK)
darwin_CXX=clang++ -target $(host) -mmacosx-version-min=$(OSX_MIN_VERSION) --sysroot $(OSX_SDK)
darwin_AR=$(host)-ar
darwin_RANLIB=$(host)-ranlib
darwin_LIBTOOL=$(host)-libtool
darwin_INSTALL_NAME_TOOL=$(host)-install_name_tool
darwin_OTOOL=$(host)-install_name_tool
darwin_native_toolchain=native_cctools
darwin_packages_for_build=native_libuuid native_openssl native_cctools native_cdrkit  native_libdmg-hfsplus
x86_64_darwin_revision=1
