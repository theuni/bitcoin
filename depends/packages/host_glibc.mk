package=host_glibc
$(package)_version=2.24
$(package)_download_path=http://ftpmirror.gnu.org/glibc
$(package)_file_name=glibc-$($(package)_version).tar.xz
$(package)_sha256_hash=99d4a3e8efd144d71488e478f62587578c0f4e1fa0b4eed47ee3d4975ebeb5d3
$(package)_build_subdir=build
$(package)_dependencies=host_gcc host_binutils
$(package)_linux_dependencies=host_linux_system_headers

define $(package)_set_vars
  $(package)_config_opts=--prefix=$($(package)_prefix)/$(host)
  $(package)_config_opts+=--host=$(host)
  $(package)_config_opts+=--enable-kernel=2.6.32
  $(package)_config_opts+=--with-headers=$($(package)_prefix)/$(host)/include
  $(package)_config_opts+=--disable-multi-arch
  $(package)_config_opts+=--enable-static-nss
  $(package)_config_opts+=--disable-werror
  $(package)_config_opts+=--without-fp
endef

define $(package)_config_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  ../configure $$($(package)_config_opts) CFLAGS=$($(package)_cflags) CXXFLAGS=$($(package)_cxxflags)
endef

define $(package)_build_cmds
  export PATH=$($(package)_prefix)/bin:$(PATH) && \
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/$(host)/lib && \
  ln -rs $($(package)_staging_prefix_dir)/lib $($(package)_staging_prefix_dir)/lib64 && ln -rs $($(package)_staging_prefix_dir)/$(host)/lib $($(package)_staging_prefix_dir)/$(host)/lib64 && \
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
