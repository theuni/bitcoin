PACKAGE=native_qttools
$(package)_version=$(native_qtbase_version)
$(package)_download_path=http://download.qt.io/official_releases/qt/5.7/$($(package)_version)/submodules
$(package)_suffix=opensource-src-$($(package)_version).tar.gz
$(package)_file_name=qttools-opensource-src-$($(package)_version).tar.gz
$(package)_sha256_hash=4d366356564505ce273e6d5be0ca86dc7aba85db69b6e8b499d901eb10df3e5c
$(package)_dependencies=native_qtbase
$(package)_qttools_sha256_hash=4d366356564505ce273e6d5be0ca86dc7aba85db69b6e8b499d901eb10df3e5c

define $(package)_preprocess_cmds
  sed -i.old '1s/^/CONFIG += force_bootstrap\n/' src/linguist/lrelease/lrelease.pro
endef

define $(package)_config_cmds
  cd src/linguist/lrelease && $($(package)_prefixbin)/qmake
endef

define $(package)_build_cmds
  $(MAKE) -C src/linguist/lrelease
endef

define $(package)_stage_cmds
  $(MAKE) INSTALL_ROOT=$($(package)_staging_dir) -C src/linguist/lrelease install_target && false
endef

define $(package)_postprocess_cmds
  rm -f lib/lib*.la lib/*.prl
endef
