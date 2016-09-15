
linux_build_toolchain = native_binutils native_glibc native_linux_system_headers native_gcc native_libstdc++ native_binutils_stage2 native_gcc_stage2 native_gcc_stage3 native_toolchain_gcc

linux_host_toolchain = host_gcc host_binutils host_linux_system_headers host_glibc host_toolchain_gcc host_gcc_stage2

mingw32_host_toolchain = host_gcc host_binutils host_mingw32_system_headers host_mingw32_crt host_mingw32_pthreads host_gcc_stage2 host_mingw32_crt_stage2 host_toolchain_gcc

native_packages = native_ccache
packages:=boost openssl libevent zeromq

qt_native_packages = native_protobuf
qt_packages = qrencode protobuf

qt_x86_64_linux_packages:=qt expat dbus libxcb xcb_proto libXau xproto freetype fontconfig libX11 xextproto libXext xtrans
qt_i686_linux_packages:=$(qt_x86_64_linux_packages)

qt_darwin_packages=qt
qt_mingw32_packages=qt

wallet_packages=bdb

upnp_packages=miniupnpc

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif

ifeq ($(build_os),linux)
build_toolchain_packages = native_toolchain_gcc
endif

ifeq ($(host_os),linux)
host_toolchain_packages = host_toolchain_gcc
endif
