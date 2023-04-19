package=zeromq
$(package)_version=4.3.4
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=c593001a89f5a85dd2ddf564805deb860e02471171b3f204944857336295c3e5
$(package)_patches=remove_libstd_link.patch netbsd_kevent_void.patch
$(package)_dependencies=native_cmake
$(package)_build_subdir=build

define $(package)_set_vars
  $(package)_cmake_opts = -DZMQ_BUILD_TESTS=OFF -DWITH_PERF_TOOL=OFF -DWITH_LIBSODIUM=OFF
  $(package)_cmake_opts += -DWITH_LIBBSD=OFF -DENABLE_CURVE=OFF -DENABLE_CPACK=OFF
  $(package)_cmake_opts += -DBUILD_SHARED=OFF -DBUILD_TESTS=OFF -DZMQ_BUILD_TESTS=OFF
  $(package)_cmake_opts += -DWITH_DOCS=OFF
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove_libstd_link.patch && \
  patch -p1 < $($(package)_patch_dir)/netbsd_kevent_void.patch
endef

define $(package)_config_cmds
   $($(package)_cmake) -S .. -B .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf bin share lib/*.la
endef
