package=native_clang
$(package)_version=3.5.0
$(package)_download_path=http://llvm.org/releases/3.5.0
$(package)_download_file=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_file_name=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_sha256_hash=b9b420b93d7681bb2b809c3271ebdf4389c9b7ca35a781c7189d07d483d8f201

define $(package)_set_vars
endef

define $(package)_preprocess_cmds
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/3.5.0/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin $($(package)_staging_prefix_dir)/include&& \
  cp -P bin/clang bin/clang++ $($(package)_staging_prefix_dir)/bin/ &&\
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -rf lib/clang/3.5.0/include/* $($(package)_staging_prefix_dir)/lib/clang/3.5.0/include/ && \
  cp -rf include/c++/ $($(package)_staging_prefix_dir)/include/ && \
  echo "#!/bin/sh" > $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  echo "exit 0" >> $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  chmod +x $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil
endef

