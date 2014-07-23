package=pcre
$(package)_version=8.35
$(package)_download_path=http://sourceforge.net/projects/pcre/files/pcre/$($(package)_version)
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=1c9ee292943ba2737f127b481a3f72f44c13fbd09a7b5b4eb8fa58650cfa56a0
$(package)_revision=1

define $(package)_set_vars
$(package)_config_opts=--disable-shared --disable-pcregrep-libz --disable-pcregrep-libbz2 --disable-pcretest-libedit --disable-pcretest-libreadline
$(package)_config_opts+=--enable-pcre16 --enable-jit
$(package)_config_opts_x86_64_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libpcre.la libpcrecpp.la libpcreposix.la libpcre16.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_destdir) install-includeHEADERS install-libLTLIBRARIES install-nodist_includeHEADERS install-pkgconfigDATA
endef
