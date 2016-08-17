PACKAGE=qt_translations
$(package)_version=$(native_qtbase_version)
$(package)_download_path=http://download.qt.io/official_releases/qt/5.7/$($(package)_version)/submodules
$(package)_suffix=opensource-src-$($(package)_version).tar.gz
$(package)_file_name=qttranslations-opensource-src-$($(package)_version).tar.gz
$(package)_sha256_hash=5769a577541d89dcf6cf7bd7e75018e02c74796d587af125e949e5adba29eee6
$(package)_dependencies=native_qtbase
$(package)_build_subdir=translations

define $(package)_config_cmds
  $(build_prefix)/bin/qmake translations.pro
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -f lib/lib*.la lib/*.prl
endef
