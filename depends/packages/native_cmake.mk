package=native_cmake
$(package)_version=3.28.1
$(package)_download_path=https://github.com/Kitware/CMake/archive/refs/tags
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=74a61650997b2a1ec17b9c6de460125e826dbd0b0e40cc1433d3ad377c8ef09c
$(package)_config_env = MAKE=$(MAKE)
$(package)_config_opts  = --no-system-libs --prefix=$(build_prefix)
$(package)_config_opts += CFLAGS="$($(package)_cflags) $($(package)_cppflags)"
$(package)_config_opts += CXXFLAGS="$($(package)_cxxflags) $($(package)_cppflags)"
$(package)_config_opts += LDFLAGS="$($(package)_ldflags)"
$(package)_config_opts += CC="$($(package)_cc)"
$(package)_config_opts += CXX="$($(package)_cxx)"

define $(package)_config_cmds
  ./bootstrap $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
