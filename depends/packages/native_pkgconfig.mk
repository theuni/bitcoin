package=native_pkgconfig
$(package)_version=0.29.2
$(package)_download_path=https://pkg-config.freedesktop.org/releases
$(package)_file_name=pkg-config-$($(package)_version).tar.gz
$(package)_sha256_hash=6fc69c01688c9458a57eb9a1664c9aba372ccda420a02bf4429fe610e7e7d591
$(package)_config_opts=--with-internal-glib --disable-shared

define $(package)_config_cmds
   $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
