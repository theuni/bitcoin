package=native_cmake
$(package)_version=3.13.5
$(package)_download_path=https://github.com/Kitware/CMake/archive/refs/tags
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=e3e3bc71254b0aca726979fb433ad124b383ab1117e51e059950a8fb63bfebf9
$(package)_patches=0001-bootstrap-don-t-over-quote-compiler-variables.patch 0002-bootstrap-correctly-deal-with-CC-CXX-env-vars-with-s.patch
$(package)_config_env = MAKE=$(MAKE)
$(package)_config_opts  = --no-system-libs --prefix=$(build_prefix)
$(package)_config_opts += CFLAGS="$($(package)_cflags) $($(package)_cppflags)"
$(package)_config_opts += CXXFLAGS="$($(package)_cxxflags) $($(package)_cppflags)"
$(package)_config_opts += LDFLAGS="$($(package)_ldflags)"
$(package)_config_opts += CC="$($(package)_cc)"
$(package)_config_opts += CXX="$($(package)_cxx)"

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/0001-bootstrap-don-t-over-quote-compiler-variables.patch && \
  patch -p1 < $($(package)_patch_dir)/0002-bootstrap-correctly-deal-with-CC-CXX-env-vars-with-s.patch
endef

define $(package)_config_cmds
  ./bootstrap $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
