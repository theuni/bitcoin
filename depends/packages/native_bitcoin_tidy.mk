package=native_bitcoin_tidy
$(package)_version=aa62f3e2279ef4b9cbba7a6e2dc6185fefb39a23
$(package)_download_path=https://github.com/theuni/bitcoin-tidy-experiments/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=34a4eabfca8a02f47d61531b6262ade3d4df53e281c6c69c687f914eb978475d
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
