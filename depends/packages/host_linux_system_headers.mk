package=host_linux_kernel_headers
$(package)_version=3.2.82
$(package)_download_path=https://www.kernel.org/pub/linux/kernel/v3.x
$(package)_file_name=linux-$($(package)_version).tar.xz
$(package)_sha256_hash=7fcb68199f5bddbe074ef3c220b1a27cf8cb38d41413dee2d6c289bb7c0fd7ec
$(package)_mingw32_dependencies=native_mingw-w64-headers

define $(package)_stage_cmds
  $(MAKE) ARCH=$(host_arch) INSTALL_HDR_PATH=$($(package)_staging_prefix_dir)/$(host) headers_install
endef
