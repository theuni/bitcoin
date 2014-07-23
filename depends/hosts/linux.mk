linux_CC=$(host_toolchain)gcc
linux_CXX=$(host_toolchain)g++
linux_AR=$(host_toolchain)ar
linux_RANLIB=$(host_toolchain)ranlib
linux_STRIP=$(host_toolchain)strip

i686_linux_CC=gcc -m32
i686_linux_CXX=g++ -m32
i686_linux_AR=ar
i686_linux_RANLIB=ranlib
i686_linux_STRIP=strip

x86_64_linux_CC=gcc
x86_64_linux_CXX=g++
x86_64_linux_AR=ar
x86_64_linux_RANLIB=ranlib
x86_64_linux_STRIP=strip

linux_revision=1
linux_packages=libxcb xcb_proto libXau xproto dbus expat freetype fontconfig libX11 xextproto libXext xtrans
