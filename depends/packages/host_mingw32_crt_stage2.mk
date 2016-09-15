package=host_mingw32_crt_stage2
#$(package)_version=4.0.6
#$(package)_sha256_hash=0c407394b0d8635553f4fbca674cdfe446aac223e90b4010603d863e4bdd015c
#$(package)_version=4.0.2
#$(package)_sha256_hash=3e9050a8c6689ef8a0cfafa40a7653e8c347cf93c105d547239c573afe7b8952
$(package)_version=4.0.4
$(package)_sha256_hash=89356a0aa8cf9f8b9dc8d92bc8dd01a131d4750c3acb30c6350a406316c42199
$(package)_download_path=https://sourceforge.net/projects/mingw-w64/files/mingw-w64/mingw-w64-release
$(package)_file_name=mingw-w64-v$($(package)_version).tar.bz2
$(package)_dependencies=host_gcc host_mingw32_system_headers host_binutils
$(package)_build_subdir=mingw-w64-crt

define $(package)_set_vars
  $(package)_config_opts=--prefix=$($(package)_prefix)/$(host)
  $(package)_config_opts+=--host=$(host)
endef

define $(package)_config_cmds
  export PATH="$($(package)_prefixbin):$(PATH)" && \
 ./configure $($(package)_config_opts) CFLAGS="-O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2"
endef

define $(package)_build_cmds
  export PATH="$($(package)_prefixbin):$(PATH)" && \
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir) && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
