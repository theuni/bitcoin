PACKAGE=native_qtbase
$(package)_version=5.7.0
$(package)_download_path=http://download.qt.io/official_releases/qt/5.7/$($(package)_version)/submodules
$(package)_suffix=opensource-src-$($(package)_version).tar.gz
$(package)_file_name=qtbase-$($(package)_suffix)
$(package)_sha256_hash=3520a3979b139a7714cb0a2a6e8b61f8cd892872abf473f91b7b05c21eff709c

$(package)_qttools_file_name=qttools-$($(package)_suffix)
$(package)_qttools_sha256_hash=4d366356564505ce273e6d5be0ca86dc7aba85db69b6e8b499d901eb10df3e5c

$(package)_extra_sources += $($(package)_qttools_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttools_file_name),$($(package)_qttools_file_name),$($(package)_qttools_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttools_sha256_hash)  $($(package)_source_dir)/$($(package)_qttools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir qtbase && \
  tar --strip-components=1 -xf $($(package)_source) -C qtbase && \
  mkdir qttools && \
  tar --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttools_file_name) -C qttools
endef

define $(package)_set_vars
$(package)_config_opts_release = -release
$(package)_config_opts_debug   = -debug
$(package)_config_opts += -c++std c++11
$(package)_config_opts += -confirm-license
$(package)_config_opts += -opensource
$(package)_config_opts += -pch
$(package)_config_opts += -prefix $(host_prefix)
$(package)_config_opts += -hostprefix $(build_prefix)
$(package)_config_opts += -hostdatadir $(build_prefix)/share
$(package)_config_opts += -reduce-exports
$(package)_config_opts += -optimized-qmake
$(package)_config_opts += -static
$(package)_config_opts += -silent

$(package)_config_opts += -nomake examples
$(package)_config_opts += -nomake tests

$(package)_config_opts += -no-accessibility
$(package)_config_opts += -no-alsa
$(package)_config_opts += -no-audio-backend
$(package)_config_opts += -no-compile-examples
$(package)_config_opts += -no-cups
$(package)_config_opts += -no-dbus
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
$(package)_config_opts += -nomake examples
$(package)_config_opts += -nomake tests
$(package)_config_opts += -no-mirclient
$(package)_config_opts += -no-mitshm
$(package)_config_opts += -no-mtdev
$(package)_config_opts += -no-nis
$(package)_config_opts += -no-opengl
$(package)_config_opts += -no-openssl
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
$(package)_config_opts += -no-xkbcommon
$(package)_config_opts += -no-xkbcommon-evdev
$(package)_config_opts += -no-xrender

$(package)_config_opts += -qt-pcre
$(package)_config_opts += -qt-zlib

endef

define $(package)_preprocess_cmds
  sed -i.old '1s/^/CONFIG += force_bootstrap\n/' qtbase/src/tools/uic/uic.pro && \
  sed -i.old '1s/^/CONFIG += force_bootstrap\n/' qtbase/src/tools/qdbusxml2cpp/qdbusxml2cpp.pro && \
  sed -i.old '1s/^/CONFIG += force_bootstrap\n/' qttools/src/linguist/lrelease/lrelease.pro && \
  sed -i.old '1s/^/CONFIG += force_bootstrap\n/' qttools/src/linguist/lupdate/lupdate.pro
endef

define $(package)_config_cmds
  cd qtbase && \
  MAKE=$(MAKE) ./configure $($(package)_config_opts) && \
  cd $($(package)_extract_dir)/qtbase && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/bootstrap && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/rcc && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/uic && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/moc && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/bootstrap-dbus && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/src/tools/qdbusxml2cpp && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qttools/src/linguist/lrelease && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qttools/src/linguist/lupdate && $($(package)_extract_dir)/qtbase/bin/qmake && \
  cd $($(package)_extract_dir)/qtbase/qmake && $($(package)_extract_dir)/qtbase/bin/qmake qmake-aux.pro
endef


define $(package)_build_cmds
  $(MAKE) -C qtbase/src/tools/bootstrap && \
  $(MAKE) -C qtbase/src/tools/rcc && \
  $(MAKE) -C qtbase/src/tools/moc && \
  $(MAKE) -C qtbase/src/tools/uic && \
  $(MAKE) -C qtbase/src/tools/bootstrap-dbus && \
  $(MAKE) -C qtbase/src/tools/qdbusxml2cpp && \
  $(MAKE) -C qttools/src/linguist/lrelease && \
  $(MAKE) -C qttools/src/linguist/lupdate
endef


define $(package)_stage_cmds
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase/src/tools/uic install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase/src/tools/rcc install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase/src/tools/moc install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase/src/tools/qdbusxml2cpp install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase/qmake install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qttools/src/linguist/lrelease install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qttools/src/linguist/lupdate install && \
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C qtbase install_mkspecs
endef

define $(package)_postprocess_cmds
  rm -f lib/lib*.la lib/*.prl
endef
