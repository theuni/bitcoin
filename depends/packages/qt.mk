PACKAGE=qt
$(package)_version=5.9.1
$(package)_download_path=http://qt.mirror.constant.com/archive/qt/5.9/5.9.1/submodules/
$(package)_suffix=opensource-src-$($(package)_version).tar.xz
$(package)_file_name=qtbase-$($(package)_suffix)
$(package)_sha256_hash=bc9a21e9f6fff9629019fdf9f989f064751d5073c3a28dc596def92f4d4275c6
$(package)_dependencies=openssl
$(package)_linux_dependencies=freetype fontconfig libxcb libX11 xproto libXext
$(package)_build_subdir=qtbase
$(package)_qt_libs=corelib network widgets gui plugins testlib
$(package)_patches=mac-qmake.conf fix_qt_pkgconfig.patch fix-xcb-include-order.patch fix_print_include.h

$(package)_qttranslations_file_name=qttranslations-$($(package)_suffix)
$(package)_qttranslations_sha256_hash=4a12528a14ed77f31672bd7469cad30624e7b672f241b8f19ad59510298eb269


$(package)_qttools_file_name=qttools-$($(package)_suffix)
$(package)_qttools_sha256_hash=c4eb56cf24a75661b8317b566be37396c90357b4f6730ef12b8c97a7079ca0e8

$(package)_extra_sources  = $($(package)_qttranslations_file_name)
$(package)_extra_sources += $($(package)_qttools_file_name)

define $(package)_set_vars
$(package)_config_opts_release = -release
$(package)_config_opts_debug = -debug
$(package)_config_opts += -bindir $(build_prefix)/bin
$(package)_config_opts += -c++std c++11
$(package)_config_opts += -confirm-license
$(package)_config_opts += -dbus-runtime
$(package)_config_opts += -hostprefix $(build_prefix)
$(package)_config_opts += -no-cups
$(package)_config_opts += -no-egl
$(package)_config_opts += -no-eglfs
$(package)_config_opts += -no-freetype
$(package)_config_opts += -no-gif
$(package)_config_opts += -no-glib
$(package)_config_opts += -no-icu
$(package)_config_opts += -no-iconv
$(package)_config_opts += -no-kms
$(package)_config_opts += -no-linuxfb
$(package)_config_opts += -no-libudev
$(package)_config_opts += -no-mtdev
$(package)_config_opts += -no-openvg
$(package)_config_opts += -no-reduce-relocations
$(package)_config_opts += -no-qml-debug
$(package)_config_opts += -no-sql-db2
$(package)_config_opts += -no-sql-ibase
$(package)_config_opts += -no-sql-oci
$(package)_config_opts += -no-sql-tds
$(package)_config_opts += -no-sql-mysql
$(package)_config_opts += -no-sql-odbc
$(package)_config_opts += -no-sql-psql
$(package)_config_opts += -no-sql-sqlite
$(package)_config_opts += -no-sql-sqlite2
$(package)_config_opts += -no-use-gold-linker
$(package)_config_opts += -no-xinput2
$(package)_config_opts += -nomake examples
$(package)_config_opts += -nomake tests
$(package)_config_opts += -opensource
$(package)_config_opts += -openssl-linked
$(package)_config_opts += -optimized-qmake
$(package)_config_opts += -pch
$(package)_config_opts += -pkg-config
$(package)_config_opts += -prefix $(host_prefix)
$(package)_config_opts += -qt-libpng
$(package)_config_opts += -no-libjpeg
$(package)_config_opts += -qt-pcre
$(package)_config_opts += -qt-zlib
$(package)_config_opts += -reduce-exports
$(package)_config_opts += -static
$(package)_config_opts += -silent
$(package)_config_opts += -v
$(package)_config_opts += -make libs
$(package)_config_opts += -no-compile-examples


$(package)_config_opts += -feature-abstractbutton
$(package)_config_opts += -feature-abstractslider
$(package)_config_opts += -feature-accessibility
$(package)_config_opts += -feature-action
$(package)_config_opts += -feature-animation
$(package)_config_opts += -no-feature-appstore-compliant
$(package)_config_opts += -no-feature-bearermanagement
$(package)_config_opts += -no-feature-big_codecs
$(package)_config_opts += -feature-buttongroup
$(package)_config_opts += -feature-calendarwidget
$(package)_config_opts += -feature-checkbox
$(package)_config_opts += -feature-clipboard
$(package)_config_opts += -no-feature-codecs
$(package)_config_opts += -no-feature-colordialog
$(package)_config_opts += -no-feature-colornames
$(package)_config_opts += -no-feature-columnview
$(package)_config_opts += -feature-combobox
$(package)_config_opts += -no-feature-commandlineparser
$(package)_config_opts += -no-feature-commandlinkbutton
$(package)_config_opts += -feature-completer
$(package)_config_opts += -no-feature-concurrent
$(package)_config_opts += -feature-contextmenu
$(package)_config_opts += -feature-cssparser
$(package)_config_opts += -no-feature-cups
$(package)_config_opts += -feature-cursor
$(package)_config_opts += -feature-datawidgetmapper
$(package)_config_opts += -feature-datestring
$(package)_config_opts += -feature-datetimeedit
$(package)_config_opts += -feature-desktopservices
$(package)_config_opts += -no-feature-dial
$(package)_config_opts += -feature-dialog
$(package)_config_opts += -feature-dialogbuttonbox
$(package)_config_opts += -feature-dirmodel
$(package)_config_opts += -no-feature-dockwidget
$(package)_config_opts += -no-feature-dom
$(package)_config_opts += -feature-draganddrop
$(package)_config_opts += -no-feature-effects
$(package)_config_opts += -no-feature-errormessage
$(package)_config_opts += -feature-filedialog
$(package)_config_opts += -no-feature-filesystemiterator
$(package)_config_opts += -feature-filesystemmodel
$(package)_config_opts += -no-feature-filesystemwatcher
$(package)_config_opts += -no-feature-fontcombobox
$(package)_config_opts += -no-feature-fontdialog
$(package)_config_opts += -feature-formlayout
$(package)_config_opts += -feature-freetype
$(package)_config_opts += -no-feature-fscompleter
$(package)_config_opts += -no-feature-ftp
$(package)_config_opts += -no-feature-gestures
$(package)_config_opts += -no-feature-graphicseffect
$(package)_config_opts += -no-feature-graphicsview
$(package)_config_opts += -feature-groupbox
$(package)_config_opts += -no-feature-highdpiscaling
$(package)_config_opts += -feature-http
$(package)_config_opts += -no-feature-iconv
$(package)_config_opts += -no-feature-identityproxymodel
$(package)_config_opts += -no-feature-im
$(package)_config_opts += -no-feature-image_heuristic_mask
$(package)_config_opts += -no-feature-image_text
$(package)_config_opts += -no-feature-imageformat_bmp
$(package)_config_opts += -no-feature-imageformat_jpeg
$(package)_config_opts += -feature-imageformat_png
$(package)_config_opts += -no-feature-imageformat_ppm
$(package)_config_opts += -no-feature-imageformat_xbm
$(package)_config_opts += -feature-imageformat_xpm
$(package)_config_opts += -no-feature-imageformatplugin
$(package)_config_opts += -no-feature-inputdialog
$(package)_config_opts += -feature-itemmodel
$(package)_config_opts += -feature-itemviews
$(package)_config_opts += -no-feature-keysequenceedit
$(package)_config_opts += -feature-label
$(package)_config_opts += -no-feature-lcdnumber
$(package)_config_opts += -feature-library
$(package)_config_opts += -feature-lineedit
$(package)_config_opts += -feature-listview
$(package)_config_opts += -no-feature-listwidget
$(package)_config_opts += -feature-localserver
$(package)_config_opts += -feature-mainwindow
$(package)_config_opts += -no-feature-mdiarea
$(package)_config_opts += -feature-menu
$(package)_config_opts += -feature-menubar
$(package)_config_opts += -feature-messagebox
$(package)_config_opts += -no-feature-mimetype # osx
$(package)_config_opts += -no-feature-movie
$(package)_config_opts += -no-feature-networkdiskcache
$(package)_config_opts += -no-feature-networkinterface
$(package)_config_opts += -feature-networkproxy
$(package)_config_opts += -no-feature-paint_debug
$(package)_config_opts += -no-feature-pdf #osx
$(package)_config_opts += -no-feature-picture #osx
$(package)_config_opts += -no-feature-printdialog
$(package)_config_opts += -no-feature-printer #osx
$(package)_config_opts += -no-feature-printpreviewdialog
$(package)_config_opts += -no-feature-printpreviewwidget
$(package)_config_opts += -feature-process
$(package)_config_opts += -feature-processenvironment
$(package)_config_opts += -feature-progressbar
$(package)_config_opts += -feature-progressdialog
$(package)_config_opts += -feature-properties
$(package)_config_opts += -feature-proxymodel
$(package)_config_opts += -feature-pushbutton
$(package)_config_opts += -feature-radiobutton
$(package)_config_opts += -no-feature-regularexpression
$(package)_config_opts += -feature-resizehandler
$(package)_config_opts += -feature-rubberband
$(package)_config_opts += -feature-scrollarea
$(package)_config_opts += -feature-scrollbar
$(package)_config_opts += -no-feature-scroller
$(package)_config_opts += -no-feature-sessionmanager
$(package)_config_opts += -feature-settings
$(package)_config_opts += -no-feature-sha3-fast
$(package)_config_opts += -no-feature-sharedmemory
$(package)_config_opts += -feature-shortcut
$(package)_config_opts += -no-feature-sizegrip
$(package)_config_opts += -feature-slider

$(package)_config_opts += -no-feature-socks5
$(package)_config_opts += -feature-sortfilterproxymodel
$(package)_config_opts += -feature-spinbox
$(package)_config_opts += -feature-splashscreen
$(package)_config_opts += -feature-splitter
$(package)_config_opts += -feature-stackedwidget
$(package)_config_opts += -feature-standarditemmodel
$(package)_config_opts += -no-feature-statemachine
$(package)_config_opts += -feature-statusbar
$(package)_config_opts += -no-feature-statustip
$(package)_config_opts += -feature-stringlistmodel
$(package)_config_opts += -feature-style-stylesheet
$(package)_config_opts += -no-feature-syntaxhighlighter
$(package)_config_opts += -no-feature-systemsemaphore
$(package)_config_opts += -feature-systemtrayicon
$(package)_config_opts += -feature-tabbar
$(package)_config_opts += -feature-tabletevent
$(package)_config_opts += -feature-tableview
$(package)_config_opts += -feature-tablewidget
$(package)_config_opts += -feature-tabwidget

$(package)_config_opts += -feature-temporaryfile
$(package)_config_opts += -no-feature-textbrowser
$(package)_config_opts += -feature-textcodec
$(package)_config_opts += -feature-textdate
$(package)_config_opts += -feature-textedit
$(package)_config_opts += -feature-texthtmlparser
$(package)_config_opts += -no-feature-textodfwriter
$(package)_config_opts += -feature-timezone
$(package)_config_opts += -feature-toolbar
$(package)_config_opts += -no-feature-toolbox
$(package)_config_opts += -feature-toolbutton
$(package)_config_opts += -feature-tooltip
$(package)_config_opts += -no-feature-topleveldomain
$(package)_config_opts += -feature-translation
$(package)_config_opts += -feature-treeview
$(package)_config_opts += -feature-treewidget
$(package)_config_opts += -no-feature-udpsocket
$(package)_config_opts += -no-feature-undocommand
$(package)_config_opts += -no-feature-undogroup
$(package)_config_opts += -no-feature-undostack
$(package)_config_opts += -no-feature-undoview
$(package)_config_opts += -feature-validator
$(package)_config_opts += -no-feature-whatsthis
$(package)_config_opts += -no-feature-wheelevent
$(package)_config_opts += -feature-widgettextcontrol
$(package)_config_opts += -no-feature-wizard
$(package)_config_opts += -feature-xmlstream
$(package)_config_opts += -feature-xmlstreamreader
$(package)_config_opts += -feature-xmlstreamwriter
#$(package)_config_opts += -no-feature-xml
$(package)_config_opts += -no-feature-sql

$(package)_config_opts += -no-feature-style-mac
ifneq ($(build_os),darwin)
$(package)_config_opts_darwin = -xplatform macx-clang-linux
$(package)_config_opts_darwin += -device-option MAC_SDK_PATH=$(OSX_SDK)
$(package)_config_opts_darwin += -device-option MAC_SDK_VERSION=$(OSX_SDK_VERSION)
$(package)_config_opts_darwin += -device-option CROSS_COMPILE="$(host)-"
$(package)_config_opts_darwin += -device-option MAC_MIN_VERSION=$(OSX_MIN_VERSION)
$(package)_config_opts_darwin += -device-option MAC_TARGET=$(host)
$(package)_config_opts_darwin += -device-option MAC_LD64_VERSION=$(LD64_VERSION)
endif

$(package)_config_opts_linux  = -qt-xkbcommon
$(package)_config_opts_linux += -qt-xcb
$(package)_config_opts_linux += -system-freetype
$(package)_config_opts_linux += -no-sm
$(package)_config_opts_linux += -fontconfig
$(package)_config_opts_linux += -no-opengl
$(package)_config_opts_arm_linux  = -platform linux-g++ -xplatform $(host)
$(package)_config_opts_i686_linux  = -xplatform linux-g++-32
$(package)_config_opts_mingw32  = -no-opengl -xplatform win32-g++ -device-option CROSS_COMPILE="$(host)-"
$(package)_build_env  = QT_RCC_TEST=1
endef

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttranslations_file_name),$($(package)_qttranslations_file_name),$($(package)_qttranslations_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttools_file_name),$($(package)_qttools_file_name),$($(package)_qttools_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttranslations_sha256_hash)  $($(package)_source_dir)/$($(package)_qttranslations_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttools_sha256_hash)  $($(package)_source_dir)/$($(package)_qttools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir qtbase && \
  tar --strip-components=1 -xf $($(package)_source) -C qtbase && \
  mkdir qttranslations && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttranslations_file_name) -C qttranslations && \
  mkdir qttools && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttools_file_name) -C qttools
endef


define $(package)_preprocess_cmds
  sed -i.old "s|updateqm.commands = \$$$$\$$$$LRELEASE|updateqm.commands = $($(package)_extract_dir)/qttools/bin/lrelease|" qttranslations/translations/translations.pro && \
  sed -i.old "/updateqm.depends =/d" qttranslations/translations/translations.pro && \
  sed -i.old 's/if \[ "$$$$XPLATFORM_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/if \[ "$$$$BUILD_ON_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/' qtbase/configure && \
  mkdir -p qtbase/mkspecs/macx-clang-linux &&\
  cp -f qtbase/mkspecs/macx-clang/Info.plist.lib qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/Info.plist.app qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/qplatformdefs.h qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f $($(package)_patch_dir)/mac-qmake.conf qtbase/mkspecs/macx-clang-linux/qmake.conf && \
  patch -p1 < $($(package)_patch_dir)/fix_qt_pkgconfig.patch && \
  patch -p0 < $($(package)_patch_dir)/fix_print_include.h && \
  echo "!host_build: QMAKE_CFLAGS     += $($(package)_cflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "!host_build: QMAKE_CXXFLAGS   += $($(package)_cxxflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "!host_build: QMAKE_LFLAGS     += $($(package)_ldflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  sed -i.old "s|QMAKE_CFLAGS            = |!host_build: QMAKE_CFLAGS            = $($(package)_cflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|QMAKE_LFLAGS            = |!host_build: QMAKE_LFLAGS            = $($(package)_ldflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|QMAKE_CXXFLAGS          = |!host_build: QMAKE_CXXFLAGS            = $($(package)_cxxflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf

endef

define $(package)_config_cmds
  export PKG_CONFIG_SYSROOT_DIR=/ && \
  export PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig && \
  export PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig  && \
  ./configure $($(package)_config_opts) && \
  $(MAKE) sub-src-clean && \
  cd ../qttranslations && ../qtbase/bin/qmake qttranslations.pro -o Makefile && \
  cd translations && ../../qtbase/bin/qmake translations.pro -o Makefile && cd ../.. &&\
  cd qttools/src/linguist/lrelease/ && ../../../../qtbase/bin/qmake lrelease.pro -o Makefile
endef

define $(package)_build_cmds
  $(MAKE) && \
  $(MAKE) -C ../qttools/src/linguist/lrelease && \
  $(MAKE) -C ../qttranslations
endef

define $(package)_stage_cmds
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) install && cd .. &&\
  $(MAKE) -C qttools/src/linguist/lrelease INSTALL_ROOT=$($(package)_staging_dir) install_target && \
  $(MAKE) -C qttranslations INSTALL_ROOT=$($(package)_staging_dir) install_subtargets && \
  if `test -f qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a`; then \
    cp qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a $($(package)_staging_prefix_dir)/lib; \
  fi
endef

define $(package)_postprocess_cmds
  rm -rf native/mkspecs/ native/lib/ lib/cmake/ && \
  rm -f lib/lib*.la lib/*.prl plugins/*/*.prl
endef
