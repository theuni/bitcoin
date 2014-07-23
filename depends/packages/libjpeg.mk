package=libjpeg
$(package)_version=9a
$(package)_download_path=http://www.ijg.org/files
$(package)_file_name=jpegsrc.v$($(package)_version).tar.gz
$(package)_sha256_hash=3a753ea48d917945dd54a2d97de388aa06ca2eb1066cbfdc6652036349fe05a7
$(package)_revision=1

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts_x86_64_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libjpeg.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-libLTLIBRARIES install-includeHEADERS install-data-local
endef
