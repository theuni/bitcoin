package=native_binutils_stage2
$(package)_version=2.27
$(package)_download_path=http://ftpmirror.gnu.org/binutils
$(package)_file_name=binutils-$($(package)_version).tar.bz2
$(package)_dependencies=native_binutils native_gcc_stage3
$(package)_linux_dependencies=native_glibc native_linux_system_headers
$(package)_sha256_hash=369737ce51587f92466041a97ab7d2358c6d9e1b6490b3940eb09fb0a9a6ac88

#$(build_prefix)/lib
define $(package)_set_vars
  $(package)_config_opts=--disable-nls
  $(package)_config_opts+=--disable-multilib
  $(package)_config_opts+=--disable-shared
  $(package)_config_opts+=--with-sysroot
  $(package)_config_opts+=--host=$(build)
  $(package)_config_opts+=--prefix=$($(package)_prefix)
  $(package)_config_opts+=--with-lib-path=:
  $(package)_config_opts+=--program-prefix=$(build)-
  $(package)_config_opts+=--enable-deterministic-archives
  $(package)_config_opts+=CC=$($(package)_cc)
  $(package)_config_opts+=CXX=$($(package)_cxx)
  $(package)_config_opts+=AR=$($(package)_ar)
  $(package)_config_opts+=RANLIB=$($(package)_ranlib)
  $(package)_config_opts+=NM=$($(package)_nm)
  $(package)_config_opts+=CFLAGS=$($(package)_cflags)
  $(package)_config_opts+=CXXFLAGS=$($(package)_cflags)
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
