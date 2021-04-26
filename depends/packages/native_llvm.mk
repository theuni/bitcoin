package=native_llvm
$(package)_version=16.0.0
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
ifneq (,$(findstring aarch64,$(BUILD)))
$(package)_file_name=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash=b750ba3120e6153fc5b316092f19b52cf3eb64e19e5f44bd1b962cb54a20cf0a
else
$(package)_file_name=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-18.04.tar.xz
$(package)_sha256_hash=2b8a69798e8dddeb57a186ecac217a35ea45607cb2b3cf30014431cff4340ad1
endif

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/16/include && \
  cp -r lib/clang/16/include/* $($(package)_staging_prefix_dir)/lib/clang/16/include/ && \
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cp bin/clang $($(package)_staging_prefix_dir)/bin/ && \
  cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin/ && \
  cp bin/dsymutil $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  cp bin/lld $($(package)_staging_prefix_dir)/bin/$(host)-ld && \
  cp bin/llvm-ar $($(package)_staging_prefix_dir)/bin/$(host)-ar && \
  cp bin/llvm-config $($(package)_staging_prefix_dir)/bin/llvm-config && \
  cp bin/llvm-install-name-tool $($(package)_staging_prefix_dir)/bin/$(host)-install_name_tool && \
  cp bin/llvm-libtool-darwin $($(package)_staging_prefix_dir)/bin/$(host)-libtool && \
  cp bin/llvm-nm $($(package)_staging_prefix_dir)/bin/$(host)-nm && \
  cp bin/llvm-otool $($(package)_staging_prefix_dir)/bin/$(host)-otool && \
  cp bin/llvm-ranlib $($(package)_staging_prefix_dir)/bin/$(host)-ranlib && \
  cp bin/llvm-strip $($(package)_staging_prefix_dir)/bin/$(host)-strip && \
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/
endef
