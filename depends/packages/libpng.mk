package=libpng
$(package)_version=1.6.8
$(package)_download_path=http://download.sourceforge.net/libpng
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=32c7acf1608b9c8b71b743b9780adb7a7b347563dbfb4a5263761056da44cc96
$(package)_dependencies=zlib
$(package)_revision=1

define $(package)_set_vars
$(package)_config_opts=--disable-shared
$(package)_config_opts_x86_64_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share bin
endef
