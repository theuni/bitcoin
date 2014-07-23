.NOTPARALLEL :
.PHONY: install cached

base_revision=1
BASEDIR ?= $(CURDIR)
SOURCES_PATH ?= $(BASEDIR)/sources
SDK_PATH ?= $(BASEDIR)/SDKs
PATCHES_PATH ?= $(BASEDIR)/patches
BASE_CACHE ?= $(BASEDIR)/built
BUILD ?= $(shell ./config.guess)
host:=$(BUILD)
ifneq ($(HOST),)
host:=$(HOST)
host_toolchain:=$(HOST)-
endif

HOST ?= $(BUILD)

base_build_dir=$(BASEDIR)/work/build
base_staging_dir=$(BASEDIR)/work/staging
canonical_host:=$(shell ./config.sub $(HOST))
build:=$(shell ./config.sub $(BUILD))

build_arch =$(firstword $(subst -, ,$(build)))
build_vendor=$(word 2,$(subst -, ,$(build)))
full_build_os:=$(subst $(build_arch)-$(build_vendor)-,,$(build))
build_os:=$(findstring linux,$(full_build_os))
build_os+=$(findstring darwin,$(full_build_os))
build_os:=$(strip $(build_os))
ifeq ($(build_os),)
build_os=$(full_build_os)
endif

host_arch=$(firstword $(subst -, ,$(canonical_host)))
host_vendor=$(word 2,$(subst -, ,$(canonical_host)))
full_host_os:=$(subst $(host_arch)-$(host_vendor)-,,$(canonical_host))
host_os:=$(findstring linux,$(full_host_os))
host_os+=$(findstring darwin,$(full_host_os))
host_os+=$(findstring mingw32,$(full_host_os))
host_os:=$(strip $(host_os))
ifeq ($(host_os),)
host_os=$(full_host_os)
endif

$(host_arch)_$(host_os)_prefix=$(BASEDIR)/$(host)
$(host_arch)_$(host_os)_host=$(host)
host_prefix=$($(host_arch)_$(host_os)_prefix)
build_prefix=$(host_prefix)/native
build_host=$(build)

all: cached

include hosts/$(host_os).mk
include hosts/default.mk
include builders/$(build_os).mk
include builders/default.mk

packages:=qrencode bdb libjpeg zlib libpng protobuf miniupnpc pcre boost openssl qt

native_packages := native_protobuf

include funcs.mk

toolchain_path=$($($(host_arch)_$(host_os)_native_toolchain)_prefixbin)
final_build_id:=$(shell echo -n $(final_build_id_long) | $(build_SHASUM) | cut -c-10)

$(host_prefix)/.stamp_$(final_build_id): | $(native_packages) $(packages)
	@rm -rf $(@D)
	@mkdir -p $(@D)
	@echo copying packages: $|
	@echo to: $(@D)
	cd $(@D); $(foreach package,$|, tar xf $($(package)_cached); )
	@touch $@

$(host_prefix)/share/config.site : config.site.in $(host_prefix)/.stamp_$(final_build_id)
	@mkdir -p $(@D)
	@sed -e 's|@HOST@|$(host)|' \
            -e 's|@CC@|$(toolchain_path)$(host_CC)|' \
            -e 's|@CXX@|$(toolchain_path)$(host_CXX)|' \
            -e 's|@AR@|$(toolchain_path)$(host_AR)|' \
            -e 's|@RANLIB@|$(toolchain_path)$(host_RANLIB)|' \
            -e 's|@STRIP@|$(toolchain_path)$(host_STRIP)|' \
            -e 's|@OTOOL@|$(toolchain_path)$(host_OTOOL)|' \
            -e 's|@INSTALL_NAME_TOOL@|$(toolchain_path)$(host_INSTALL_NAME_TOOL)|' \
            -e 's|@build_os@|$(build_os)|' \
            -e 's|@host_os@|$(host_os)|' \
            -e 's|@CFLAGS@|$(host_CFLAGS)|' \
            -e 's|@CXXFLAGS@|$(host_CXXFLAGS)|' \
            -e 's|@LDFLAGS@|$(host_LDFLAGS)|' \
            $< > $@
	@touch $@

install: $(host_prefix)/share/config.site
