package=native_libstdc++2
$(package)_version=6.2.0
$(package)_download_path=http://ftpmirror.gnu.org/gcc/gcc-$($(package)_version)
$(package)_file_name=gcc-$($(package)_version).tar.bz2
$(package)_sha256_hash=$(native_gcc_sha256_hash)
$(package)_build_subdir=build
$(package)_dependencies=native_binutils_stage3 native_host_gcc native_glibc native_kernel_headers
$(package)_mingw32_dependencies=native_mingw-w64-headers native_mingw-w64-crt

define $(package)_set_vars
  $(package)_config_opts=--host=$(host)
  $(package)_config_opts+=--prefix=$(build_prefix)/$(host)
  $(package)_config_opts+=--with-gxx-include-dir=$(build_prefix)/include/c++/$($(package)_version)
  $(package)_config_opts+=--disable-nls
  $(package)_config_opts+=--disable-multilib
  $(package)_config_opts+=--disable-libstdcxx-pch
  $(package)_cflags+=-fPIC
  $(package)_cxxflags+=-fPIC
endef

define $(package)_config_cmds
  export PATH=$(build_prefix)/bin:$(PATH) && \
  ../libstdc++-v3/configure $$($(package)_config_opts) CFLAGS="$($(package)_cflags)" CXXFLAGS="$($(package)_cxxflags)"
endef

define $(package)_build_cmds
  export PATH=$(build_prefix)/bin:$(PATH) && \
  $(MAKE)
endef

define $(package)_stage_cmds
  export PATH=$(build_prefix)/bin:$(PATH) && \
  mkdir -p $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/$(host)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(host)/lib $($(package)_staging_prefix_dir)/$(host)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
