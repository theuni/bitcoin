package=libevent
$(package)_version=2.1.12-stable
$(package)_download_path=https://github.com/libevent/libevent/releases/download/release-$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=92e6de1be9ec176428fd2367677e61ceffc2ee1cb119035037a27d346b0403bb
$(package)_dependencies=native_cmake
$(package)_build_subdir=build

# When building for Windows, we set _WIN32_WINNT to target the same Windows
# version as we do in configure. Due to quirks in libevents build system, this
# is also required to enable support for ipv6. See #19375.
define $(package)_set_vars
  $(package)_cmake_opts=-DEVENT__DISABLE_BENCHMARK=ON -DEVENT__DISABLE_OPENSSL=ON
  $(package)_cmake_opts+=-DEVENT__DISABLE_SAMPLES=ON -DEVENT__DISABLE_REGRESS=ON -DBUILD_TESTING=OFF
  $(package)_cmake_opts+=-DBUILD_TESTING=OFF -DEVENT__LIBRARY_TYPE=STATIC
  $(package)_cppflags+=-D_FORTIFY_SOURCE=3
  $(package)_cppflags_mingw32=-D_WIN32_WINNT=0x0601
endef

# TODO: $(package)_cmake_opts+=-DEVENT__DISABLE_DEBUG_MODE=ON
# Need to switch to using $(package)_configure_opts so that _debug works

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
  rm include/ev*.h && \
  rm include/event2/*_compat.h
endef
