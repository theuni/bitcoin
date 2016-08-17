PACKAGE=qt
$(package)_version=$(native_qtbase_version)
$(package)_download_path=http://download.qt.io/official_releases/qt/5.7/$($(package)_version)/submodules
$(package)_suffix=opensource-src-$($(package)_version).tar.gz
$(package)_file_name=qtbase-$($(package)_suffix)
$(package)_sha256_hash=$(native_qtbase_sha256_hash)
$(package)_dependencies=native_qtbase openssl
$(package)_linux_dependencies=freetype fontconfig libxcb libX11 xproto libXext
$(package)_qt_libs=corelib network gui widgets dbus platformsupport plugins testlib 3rdparty/harfbuzz-ng 3rdparty/pcre
$(package)_patches=mac-qmake.conf mingw-uuidof.patch pidlist_absolute.patch fix-xcb-include-order.patch fix_qt_pkgconfig.patch

define $(package)_set_vars
$(package)_config_opts_release = -release
$(package)_config_opts_debug   = -debug
$(package)_config_opts += -c++std c++11
$(package)_config_opts += -confirm-license
$(package)_config_opts += -opensource
$(package)_config_opts += -pch
$(package)_config_opts += -prefix $(host_prefix)
$(package)_config_opts += -hostprefix $(build_prefix)
$(package)_config_opts += -external-hostbindir $(build_prefix)/bin
$(package)_config_opts += -reduce-exports
$(package)_config_opts += -static
$(package)_config_opts += -silent

$(package)_config_opts += -nomake examples
$(package)_config_opts += -nomake tests
$(package)_config_opts += -nomake tools

$(package)_config_opts += -no-accessibility
$(package)_config_opts += -no-alsa
$(package)_config_opts += -no-audio-backend
$(package)_config_opts += -no-compile-examples
$(package)_config_opts += -no-cups
$(package)_config_opts += -no-egl
$(package)_config_opts += -no-eglfs
$(package)_config_opts += -no-evdev
$(package)_config_opts += -no-feature-style-windowsce
$(package)_config_opts += -no-feature-style-windowsmobile
$(package)_config_opts += -no-fontconfig
$(package)_config_opts += -no-freetype
$(package)_config_opts += -no-gbm
$(package)_config_opts += -no-gif
$(package)_config_opts += -no-glib
$(package)_config_opts += -no-gui
$(package)_config_opts += -no-harfbuzz
$(package)_config_opts += -no-iconv
$(package)_config_opts += -no-icu
$(package)_config_opts += -no-kms
$(package)_config_opts += -no-libjpeg
$(package)_config_opts += -no-libpng
$(package)_config_opts += -no-libproxy
$(package)_config_opts += -no-libudev
$(package)_config_opts += -no-linuxfb
$(package)_config_opts += -no-mirclient
$(package)_config_opts += -no-mitshm
$(package)_config_opts += -no-mtdev
$(package)_config_opts += -no-nis
$(package)_config_opts += -no-opengl
$(package)_config_opts += -no-openvg
$(package)_config_opts += -no-pkg-config
$(package)_config_opts += -no-pulseaudio
$(package)_config_opts += -no-qml-debug
$(package)_config_opts += -no-reduce-relocations
$(package)_config_opts += -no-sm
$(package)_config_opts += -no-sql-db2
$(package)_config_opts += -no-sql-ibase
$(package)_config_opts += -no-sql-mysql
$(package)_config_opts += -no-sql-oci
$(package)_config_opts += -no-sql-odbc
$(package)_config_opts += -no-sql-psql
$(package)_config_opts += -no-sql-sqlite
$(package)_config_opts += -no-sql-sqlite2
$(package)_config_opts += -no-sql-tds
$(package)_config_opts += -no-tslib
$(package)_config_opts += -no-use-gold-linker
$(package)_config_opts += -no-widgets
$(package)_config_opts += -no-xcb
$(package)_config_opts += -no-xcb-xlib
$(package)_config_opts += -no-xinput2
$(package)_config_opts += -no-xkb
$(package)_config_opts += -no-xkbcommon-evdev
$(package)_config_opts += -no-xrender

$(package)_config_opts += -accessibility
$(package)_config_opts += -openssl-linked
$(package)_config_opts += -pkg-config
$(package)_config_opts += -qt-libpng
$(package)_config_opts += -qt-libjpeg
$(package)_config_opts += -dbus-runtime
$(package)_config_opts += -qt-pcre
$(package)_config_opts += -qt-zlib
$(package)_config_opts_linux  = -qt-xkbcommon
$(package)_config_opts_linux += -qt-xcb
$(package)_config_opts_linux += -system-freetype
$(package)_config_opts_linux += -no-sm
$(package)_config_opts_linux += -fontconfig
$(package)_config_opts_linux += -no-opengl

ifneq ($(build_os),darwin)
$(package)_config_opts_darwin = -xplatform macx-clang-linux
$(package)_config_opts_darwin += -device-option MAC_SDK_PATH=$(OSX_SDK)
$(package)_config_opts_darwin += -device-option MAC_SDK_VERSION=$(OSX_SDK_VERSION)
$(package)_config_opts_darwin += -device-option CROSS_COMPILE="$(host)-"
$(package)_config_opts_darwin += -device-option MAC_MIN_VERSION=$(OSX_MIN_VERSION)
$(package)_config_opts_darwin += -device-option MAC_TARGET=$(host)
$(package)_config_opts_darwin += -device-option MAC_LD64_VERSION=$(LD64_VERSION)
endif

$(package)_config_opts_arm_linux  = -platform linux-g++ -xplatform $(host)
$(package)_config_opts_i686_linux  = -xplatform linux-g++-32
$(package)_config_opts_mingw32  = -no-opengl -xplatform win32-g++ -device-option CROSS_COMPILE="$(host)-"
$(package)_config_env += CFLAGS="$($(package)_cflags) $($(package)_cppflags)"
$(package)_config_env += CXXFLAGS="$($(package)_cxxflags) $($(package)_cppflags)"
$(package)_config_env += LDFLAGS="$($(package)_ldflags)"
$(package)_build_env  = QT_RCC_TEST=1
endef

define $(package)_preprocess_cmds
  sed -i.old "s/^if true; then/if false; then/" configure && \
  sed -i.old 's/if \[ "$$$$XPLATFORM_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/if \[ "$$$$BUILD_ON_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/' configure && \
  mkdir -p mkspecs/macx-clang-linux &&\
  cp -f mkspecs/macx-clang/Info.plist.lib mkspecs/macx-clang-linux/ &&\
  cp -f mkspecs/macx-clang/Info.plist.app mkspecs/macx-clang-linux/ &&\
  cp -f mkspecs/macx-clang/qplatformdefs.h mkspecs/macx-clang-linux/ &&\
  cp -f $($(package)_patch_dir)/mac-qmake.conf mkspecs/macx-clang-linux/qmake.conf && \
  patch -p1 < $($(package)_patch_dir)/pidlist_absolute.patch && \
  patch -p1 < $($(package)_patch_dir)/fix_qt_pkgconfig.patch
endef

define $(package)_config_cmds
  export PKG_CONFIG_SYSROOT_DIR=/ && \
  export PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig && \
  export PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig  && \
  cp $(build_prefix)/bin/qmake bin/ && \
  CFLAGS="$($(package)_cflags) $($(package)_cppflags)" CXXFLAGS="$($(package)_cxxflags) $($(package)_cppflags)" LDFLAGS="$($(package)_ldflags)" ./configure $($(package)_config_opts) && \
  $(foreach lib,$($(package)_qt_libs),cd $($(package)_extract_dir)/src/$(lib) && $($(package)_extract_dir)/bin/qmake -qtconf $($(package)_extract_dir)/bin/qt.conf -nodepend &&) true
endef

define $(package)_build_cmds
  $(foreach lib,$($(package)_qt_libs),$(MAKE) -C src/$(lib) &&) true
endef

define $(package)_stage_cmds
  $(foreach lib,$($(package)_qt_libs),$(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C src/$(lib) install &&) true
endef


define $(package)_postprocess_cmds
  rm -rf native/mkspecs/ native/lib/ lib/cmake/ && \
  rm -f lib/lib*.la lib/*.prl plugins/*/*.prl
endef
