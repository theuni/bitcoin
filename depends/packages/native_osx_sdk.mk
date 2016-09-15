package=native_ccache
$(package)_version=$(OSX_SDK_VERSION)
$(package)_file_name=MacOSX$($(package)_version).sdk.tar.gz
$(package)_sha256_hash=0

define $(package)_fetch_cmds
  test -f $(SOURCES_PATH)/$($(package)_file_name) || (echo no file && false)
endef

define $(package)_set_vars
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
