package=native_faketime
$(package)_version=0.9.6
$(package)_download_path=http://www.code-wizards.com/projects/libfaketime
$(package)_file_name=libfaketime-$($(package)_version).tar.gz
$(package)_sha256_hash=3a89972708e262ae3a548655a04f197b48e9c82ac2b61acaeb1c47b135a682b7

define $(package)_set_vars
endef

define $(package)_config_cmds
endef

define $(package)_build_cmds
  $(MAKE) PREFIX=$(build_prefix)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir) && \
  $(MAKE) PREFIX=$(build_prefix) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
