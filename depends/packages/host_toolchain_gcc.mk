package=host_toolchain_gcc
$(package)_version=$(native_gcc_version)
$(package)_dependencies=host_gcc_stage2 host_binutils
$(package)_linux_dependencies=host_linux_system_headers host_glibc
$(package)_mingw32_dependencies=host_mingw32_system_headers host_mingw32_crt_stage2 host_mingw32_pthreads

define $(package)_fetch_cmds
endef

define $(package)_extract_cmds
endef

define $(package)_stage_cmds
  mkdir -p host_toolchain && cp -rf $($(package)_prefix) $($(package)_staging_prefix_dir)
endef
