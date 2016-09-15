package=host_mingw32_pthreads
$(package)_version=$(host_mingw32_crt_version)
$(package)_download_path=$(host_mingw32_crt_download_path)
$(package)_file_name=$(host_mingw32_crt_file_name)
$(package)_sha256_hash=$(host_mingw32_crt_sha256_hash)
$(package)_dependencies=host_gcc host_mingw32_system_headers host_binutils host_mingw32_crt
$(package)_build_subdir=mingw-w64-libraries/winpthreads

define $(package)_set_vars
  $(package)_config_opts=--prefix=$($(package)_prefix)/$(host)
  $(package)_config_opts+=--host=$(host)
endef

define $(package)_config_cmds
  export PATH="$($(package)_prefixbin):$(PATH)" && \
 ./configure $($(package)_config_opts) CFLAGS="-O2"
endef

define $(package)_build_cmds
  export PATH="$($(package)_prefixbin):$(PATH)" && \
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir) && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
