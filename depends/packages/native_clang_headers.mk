package=native_clang_headers
$(package)_version=14.0.0-rc2
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_file_name=llvm-project-14.0.0rc2.src.tar.xz
$(package)_sha256_hash=e728c13e56034894994eefe596d1edd97a66c798a504fabd65f63ceb6befade1
$(package)_config_opts=-S llvm
$(package)_config_opts+=-DCMAKE_BUILD_TYPE=Release
$(package)_config_opts+=-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra"
$(package)_config_opts_x86_64=-DLLVM_TARGETS_TO_BUILD=X86
$(package)_config_opts_aarch64=-DLLVM_TARGETS_TO_BUILD=AArch64
$(package)_config_opts_powerpc64=-DLLVM_TARGETS_TO_BUILD=PowerPC

define $(package)_config_cmds
  $($(package)_cmake) $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) llvm-headers clang-headers clang-tidy-headers
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-llvm-headers install-clang-headers install-clang-tidy-headers
endef
