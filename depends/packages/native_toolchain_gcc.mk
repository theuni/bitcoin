package=native_toolchain_gcc
$(package)_version=$(native_gcc_version)
$(package)_dependencies=native_binutils_stage2 native_gcc_stage3 native_glibc
$(package)_linux_dependencies=native_linux_system_headers

define $(package)_fetch_cmds
endef

define $(package)_extract_cmds
endef

define $(package)_stage_cmds
  cp -rf $($(package)_prefix) $($(package)_staging_prefix_dir)
endef
