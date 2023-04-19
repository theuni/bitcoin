package=miniupnpc
$(package)_version=2.2.2
$(package)_download_path=https://miniupnp.tuxfamily.org/files/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=888fb0976ba61518276fe1eda988589c700a3f2a69d71089260d75562afd3687
$(package)_patches=dont_leak_info.patch respect_mingw_cflags.patch
$(package)_build_subdir=build

# Next time this package is updated, ensure that _WIN32_WINNT is still properly set.
# See discussion in https://github.com/bitcoin/bitcoin/pull/25964.
define $(package)_set_vars
$(package)_cmake_opts = -DUPNPC_BUILD_SAMPLE=OFF -DUPNPC_BUILD_SHARED=OFF
$(package)_cmake_opts += -DUPNPC_BUILD_STATIC=ON -DUPNPC_BUILD_TESTS=OFF
$(package)_cppflags_mingw32=-D_WIN32_WINNT=0x0601"
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/dont_leak_info.patch && \
  patch -p1 < $($(package)_patch_dir)/respect_mingw_cflags.patch
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
