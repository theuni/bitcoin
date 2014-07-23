package=zlib
$(package)_version=1.2.8
$(package)_download_path=http://zlib.net
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d
$(package)_revision=1

define $(package)_set_vars
  $(package)_config_opts= --static --prefix=$(host_prefix)
  $(package)_config_env=RANLIB="$(host_RANLIB)" AR="$(host_AR)" CC="$(host_CC)"
  $(package)_config_env+=CFLAGS="$(host_CFLAGS) $$($(package)_cflags_$(host_os)) $$($(package)_cflags_$(host_arch)_$(host_os)) "
  $(package)_cflags_x86_64_linux+=-fPIC
endef

define $(package)_preprocess_cmds
	echo "%.o: %.c;\$$$$(CC) \$$$$(CFLAGS) -c \$$$$< -o \$$$$@" >> Makefile.in
endef

define $(package)_config_cmds
  ./configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) libz.a
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share
endef
