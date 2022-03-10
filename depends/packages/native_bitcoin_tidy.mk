package=native_bitcoin_tidy
$(package)_version=eb51df3f2b803af251b3c924942137520917a314
$(package)_download_path=https://github.com/theuni/bitcoin-tidy-experiments/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=1640c41f19b6f7b47e00fe853c148802098153e3022b1a1837494f926241093d
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
