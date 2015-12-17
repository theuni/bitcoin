package=native_dsstore
$(package)_version=c80c23706eae
$(package)_download_path=https://bitbucket.org/al45tair/ds_store/get
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=466f746ad2a38f002ad2675d5013917961a464f605f714ef573a01de98dfcfcd
$(package)_install_libdir=$(build_prefix)/lib/python/dist-packages
$(package)_dependencies=native_biplist

define $(package)_build_cmds
    python setup.py build
endef

define $(package)_stage_cmds
    mkdir -p $($(package)_install_libdir) && \
    python setup.py install --root=$($(package)_staging_dir) --prefix=$(build_prefix) --install-lib=$($(package)_install_libdir)
endef
