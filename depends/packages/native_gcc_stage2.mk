package=native_gcc_stage2
$(package)_version=6.2.0
$(package)_download_path=http://ftpmirror.gnu.org/gcc/gcc-$($(package)_version)
$(package)_file_name=gcc-$($(package)_version).tar.bz2
$(package)_sha256_hash=9944589fc722d3e66308c0ce5257788ebd7872982a718aa2516123940671b7c5
$(package)_build_subdir=build
$(package)_dependencies=native_binutils native_gcc native_libstdc++
$(package)_linux_dependencies=native_glibc native_linux_system_headers
$(package)_mingw32_dependencies=native_mingw-w64-headers native_mingw-w64-crt

$(package)_mpfr_version=3.1.4
$(package)_mpfr_download_path=http://ftpmirror.gnu.org/mpfr
$(package)_mpfr_file_name=mpfr-$($(package)_mpfr_version).tar.xz
$(package)_mpfr_sha256_hash=761413b16d749c53e2bfd2b1dfaa3b027b0e793e404b90b5fbaeef60af6517f5

$(package)_gmp_version=6.1.1
$(package)_gmp_download_path=http://ftpmirror.gnu.org/gmp
$(package)_gmp_file_name=gmp-$($(package)_gmp_version).tar.xz
$(package)_gmp_sha256_hash=d36e9c05df488ad630fff17edb50051d6432357f9ce04e34a09b3d818825e831

$(package)_mpc_version=1.0.3
$(package)_mpc_download_path=http://ftpmirror.gnu.org/mpc
$(package)_mpc_file_name=mpc-$($(package)_mpc_version).tar.gz
$(package)_mpc_sha256_hash=617decc6ea09889fb08ede330917a00b16809b8db88c29c31bfbb49cbf88ecc3

$(package)_extra_sources=$($(package)_mpfr_file_name) $($(package)_gmp_file_name) $($(package)_mpc_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_file_name),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_mpfr_download_path),$($(package)_mpfr_file_name),$($(package)_mpfr_file_name),$($(package)_mpfr_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_gmp_download_path),$($(package)_gmp_file_name),$($(package)_gmp_file_name),$($(package)_gmp_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_mpc_download_path),$($(package)_mpc_file_name),$($(package)_mpc_file_name),$($(package)_mpc_sha256_hash))
endef

define $(package)_extract_cmds
  echo "source: $($(package)_staging_dir)" && \
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_mpfr_sha256_hash)  $($(package)_source_dir)/$($(package)_mpfr_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_gmp_sha256_hash)  $($(package)_source_dir)/$($(package)_gmp_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_mpc_sha256_hash)  $($(package)_source_dir)/$($(package)_mpc_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  tar --strip-components=1 -xf $($(package)_source) && \
  mkdir -p mpfr && tar --strip-components=1 -C mpfr -xf $($(package)_source_dir)/$($(package)_mpfr_file_name) && \
  mkdir -p gmp && tar --strip-components=1 -C gmp -xf $($(package)_source_dir)/$($(package)_gmp_file_name) && \
  mkdir -p mpc && tar --strip-components=1 -C mpc -xf $($(package)_source_dir)/$($(package)_mpc_file_name)
endef

define $(package)_set_vars
  $(package)_config_opts=--enable-languages=c,c++
  $(package)_config_opts+=--with-glibc-version=2.11
  $(package)_config_opts+=--program-prefix=$(build)-
  $(package)_config_opts+=--disable-nls
  $(package)_config_opts+=--disable-multilib
  $(package)_config_opts+=--enable-linker-build-id
  $(package)_config_opts+=--without-included-gettext
  $(package)_config_opts+=--enable-libstdcxx-time=yes
  $(package)_config_opts+=--enable-fully-dynamic-string
  $(package)_config_opts+=--prefix=$($(package)_prefix)
  $(package)_config_opts+=--host=$(build)
  $(package)_config_opts+=--disable-libgomp
  $(package)_config_opts+=--disable-libstdcxx-pch
  $(package)_config_opts+=--disable-bootstrap
  $(package)_config_opts+=--with-native-system-header-dir=$($(package)_prefix)/$(build)/include
  $(package)_config_opts+=CFLAGS=$($(package)_cflags)
  $(package)_config_opts+=CXXFLAGS=$($(package)_cflags)
endef

define $(package)_config_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  ../configure $$($(package)_config_opts)
endef

define $(package)_build_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/$(build)/lib $($(package)_staging_prefix_dir)/lib/gcc/$(build)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(build)/lib $($(package)_staging_prefix_dir)/$(build)/lib64 && ln -rs $($(package)_staging_prefix_dir)/lib/gcc/$(build)/lib $($(package)_staging_prefix_dir)/lib/gcc/$(build)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -f bin/$(build)-$(build)-*
endef
