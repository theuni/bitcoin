package=native_bitcoin_tidy
$(package)_version=e9717e5120d5342faac005651d5a2cd8a7fa33fc
$(package)_download_path=https://github.com/theuni/bitcoin-tidy-experiments/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=b2a9b530673420ad1341591d9d2197abda92441e2b27806c07251292f39053ed
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
