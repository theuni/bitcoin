package=native_libstdc++
$(package)_version=6.2.0
$(package)_download_path=http://ftpmirror.gnu.org/gcc/gcc-$($(package)_version)
$(package)_file_name=gcc-$($(package)_version).tar.bz2
$(package)_sha256_hash=$(native_gcc_sha256_hash)
$(package)_build_subdir=build
$(package)_dependencies=native_binutils native_gcc
$(package)_linux_dependencies=native_glibc native_linux_system_headers
$(package)_mingw32_dependencies=native_mingw-w64-headers native_mingw-w64-crt

define $(package)_set_vars
  $(package)_config_opts=--host=$(build)
  $(package)_config_opts+=--prefix=$($(package)_prefix)/$(build)
  $(package)_config_opts+=--with-gxx-include-dir=$($(package)_prefix)/include/c++/$($(package)_version)
  $(package)_config_opts+=--disable-nls
  $(package)_config_opts+=--disable-multilib
  $(package)_config_opts+=--disable-shared
  $(package)_config_opts+=--disable-libstdcxx-threads
  $(package)_config_opts+=--disable-libstdcxx-pch
  $(package)_cflags+=-fPIC
  $(package)_cxxflags+=-fPIC
endef

define $(package)_config_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  ../libstdc++-v3/configure $$($(package)_config_opts) CFLAGS="$($(package)_cflags)" CXXFLAGS="$($(package)_cxxflags)"
endef

define $(package)_build_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  $(MAKE)
endef

define $(package)_stage_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  mkdir -p $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/$(build)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(build)/lib $($(package)_staging_prefix_dir)/$(build)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
