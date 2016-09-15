package=native_binutils
$(package)_version=2.27
$(package)_download_path=http://ftpmirror.gnu.org/binutils
$(package)_file_name=binutils-$($(package)_version).tar.bz2
$(package)_sha256_hash=369737ce51587f92466041a97ab7d2358c6d9e1b6490b3940eb09fb0a9a6ac88

define $(package)_set_vars
  $(package)_config_opts=--disable-nls
  $(package)_config_opts+=--disable-multilib
  $(package)_config_opts+=--disable-shared
  $(package)_config_opts+=--target=$(build)
  $(package)_config_opts+=--prefix=$($(package)_prefix)
  $(package)_config_opts+=--with-lib-path=$($(package)_prefix)/$(build)/lib
  $(package)_config_opts+=--program-prefix=$(build)-
  $(package)_config_opts+=CC=$(old_build_toolchain_CC)
  $(package)_config_opts+=CXX=$(old_build_toolchain_CXX)
  $(package)_config_opts+=AR=$(old_build_toolchain_AR)
  $(package)_config_opts+=RANLIB=$(old_build_toolchain_RANLIB)
  $(package)_config_opts+=NM=$(old_build_toolchain_NM)
  $(package)_config_opts+=CFLAGS=$($(package)_cflags)
  $(package)_config_opts+=CXXFLAGS=$($(package)_cflags)
endef


define $(package)_config_cmds
  ./configure $$($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/$(build)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(build)/lib $($(package)_staging_prefix_dir)/$(build)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
