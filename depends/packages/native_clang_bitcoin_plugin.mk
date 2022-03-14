package=native_clang_bitcoin_plugin
$(package)_version=05ff7212e7eb5ed31c3c3474d4f6595111dd4a9f
$(package)_download_path=https://github.com/theuni/bitcoin-core-clang-plugin/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=efdf42547c0e738bb8873708958ca4c15fe932f77de1e08a1f3087cb03d79a00
$(package)_dependencies=native_clang_headers
define $(package)_config_cmds
  $($(package)_cmake)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
