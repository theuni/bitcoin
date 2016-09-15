package=native_glibc
$(package)_version=2.24
$(package)_download_path=http://ftpmirror.gnu.org/glibc
$(package)_file_name=glibc-$($(package)_version).tar.xz
#$(package)_sha256_hash=9b3a7b924e152a85a3e000e95bc9c20c1462d876f5d6de1bc0b280d956dfd6af
$(package)_sha256_hash=99d4a3e8efd144d71488e478f62587578c0f4e1fa0b4eed47ee3d4975ebeb5d3
$(package)_build_subdir=build
$(package)_dependencies=native_gcc native_binutils
$(package)_linux_dependencies=native_linux_system_headers

#$(package)_config_opts+=--build=$(build_arch)-btc-$(build_os)
define $(package)_set_vars
  $(package)_config_opts=--prefix=$($(package)_prefix)/$(build)
  $(package)_config_opts+=--host=$(build)
  $(package)_config_opts+=--enable-kernel=2.6.32
  $(package)_config_opts+=--with-headers=$($(package)_prefix)/$(build)/include
  $(package)_config_opts+=--disable-multi-arch
  $(package)_config_opts+=libc_cv_forced_unwind=yes
  $(package)_config_opts+=libc_cv_c_cleanup=yes
  $(package)_config_opts+=BUILD_CC=$(old_build_toolchain_CC)
endef

define $(package)_config_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  ../configure $$($(package)_config_opts) CFLAGS=$($(package)_cflags) CXXFLAGS=$($(package)_cxxflags)
endef

define $(package)_build_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/$(build)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(build)/lib $($(package)_staging_prefix_dir)/$(build)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  sed -i "s|$($(package)_prefix)/$(build)/lib/||g" $(build)/lib/libc.so $(build)/lib/libm.so $(build)/lib/libpthread.so
endef
#  rm -rf ./bin ./etc ./var ./sbin ./share

