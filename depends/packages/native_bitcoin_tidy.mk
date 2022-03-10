package=native_bitcoin_tidy
$(package)_version=595e9df9dc0fdf1d3897d38323248afc580ecd51
$(package)_download_path=https://github.com/theuni/bitcoin-tidy/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=78f8c45a9570f98561561ea1a4bd536674568a80e70b8cef8b68b550ff7b80d0
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
