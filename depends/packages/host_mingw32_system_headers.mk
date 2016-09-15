package=host_mingw32_system_headers
$(package)_version=$(host_mingw32_crt_version)
$(package)_download_path=$(host_mingw32_crt_download_path)
$(package)_file_name=$(host_mingw32_crt_file_name)
$(package)_sha256_hash=$(host_mingw32_crt_sha256_hash)
$(package)_dependencies=$$(build_toolchain)
$(package)_build_subdir=mingw-w64-headers

define $(package)_set_vars
  $(package)_config_opts=--prefix=$($(package)_prefix)/$(host)
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install && \
  mkdir -p $($(package)_staging_prefix_dir)
endef
