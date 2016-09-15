package=native_freebsd
$(package)_version=$(FREEBSD_VERSION)
$(package)_download_path=http://ftp.freebsd.org/pub/FreeBSD/releases/$(FREEBSD_ARCH)/$($(package)_version)-RELEASE
$(package)_download_file=base.txz
$(package)_file_name=freebsd-$(FREEBSD_ARCH)-$($(package)_version)-base.txz
$(package)_10.1_amd64_sha256_hash=2b028a894d25711ad496762622a52d74b1e32ee04693ad1cf056e3ddcdc23975
$(package)_10.1_i386_sha256_hash=8cbe70ff3022b63f19ec254b989cc61daaa56c47938a38951818e8f3d68f5a2c
$(package)_9.3_amd64_sha256_hash=eda58db8f30e502f3e1dfb3266048e73c6be65fd820e132e2cba6cf5aee50c08
$(package)_9.3_i386_sha256_hash=8ea9a5148f8fefc51881cbd96d9bfc8288cfa3bcea2e63c5c5c4c1c879527d93
$(package)_src_download_path=$($(package)_download_path)
$(package)_src_download_file=src.txz
$(package)_src_file_name=freebsd-$($(package)_version)-src.txz
$(package)_src_10.1_sha256_hash=f919287a5ef51d4f133f27c99c54f2e8054f408d3dd53bc60f4e233cc75ec03d
$(package)_src_9.3_sha256_hash=eda58db8f30e502f3e1dfb3266048e73c6be65fd820e132e2cba6cf5aee50c08
$(package)_clang_file_name=clang-llvm-$($(package)_clang_version)-amd64-Ubuntu-12.04.2.tar.gz
$(package)_clang_sha256_hash=60d8f69f032d62ef61bf527857ebb933741ec3352d4d328c5516aa520662dab7
$(package)_clang_version=3.3
$(package)_clang_download_path=http://llvm.org/releases/$($(package)_clang_version)
$(package)_clang_download_file=clang+llvm-$($(package)_clang_version)-amd64-Ubuntu-12.04.2.tar.gz
$(package)_clang_file_name=clang-llvm-$($(package)_clang_version)-amd64-Ubuntu-12.04.2.tar.gz
$(package)_clang_sha256_hash=60d8f69f032d62ef61bf527857ebb933741ec3352d4d328c5516aa520662dab7

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_$($(package)_version)_$(FREEBSD_ARCH)__sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_src_download_path),$($(package)_src_download_file),$($(package)_src_file_name),$($(package)_src_$($(package)_version)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_clang_download_path),$($(package)_clang_download_file),$($(package)_clang_file_name),$($(package)_clang_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p sysroot toolchain && \
  tar -C sysroot -xf $($(package)_source) ./lib ./usr/lib ./usr/include && \
  tar -C sysroot -xf $($(package)_source_dir)/$($(package)_src_file_name) usr/src/include && \
  tar --strip-components=1 -C toolchain -xf $($(package)_source_dir)/$($(package)_clang_file_name)
endef

define $(package)_preprocess_cmds
  cd sysroot/usr/lib && \
  ln -sf ../../lib/libgcc_s.so.* libgcc_s.so && \
  ln -sf ../../lib/libthr.so.* libthr.so && \
  ln -sf ../../lib/libz.so.* libz.so && \
  ln -sf ../../lib/libm.so.* libm.so && \
  ln -sf ../../lib/libcxxrt.so.* libcxxrt.so && \
  ln -sf ../../lib/libc.so.* libc.so
endef

define $(package)_build_cmds
endef

define $(package)_stage_cmds
  cd $($(package)_extract_dir)/toolchain && \
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin $($(package)_staging_prefix_dir)/include && \
  cp -P bin/clang bin/clang++ $($(package)_staging_prefix_dir)/bin/ &&\
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -rf lib/clang/$($(package)_clang_version)/include/* $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include/ && \
  if `test -d include/c++/`; then cp -rf include/c++/ $($(package)_staging_prefix_dir)/include/; fi && \
  if `test -d lib/c++/`; then cp -rf lib/c++/ $($(package)_staging_prefix_dir)/lib/; fi && \
  cp -rf $($(package)_extract_dir)/sysroot/ $($(package)_staging_prefix_dir)/
endef
