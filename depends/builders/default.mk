default_build_toolchain_url = https://bitcoincore.org/depends-toolchains/linux/

default_build_toolchain_CC = $(build_toolchain_prefix)/bin/$(build)-gcc
default_build_toolchain_CXX = $(build_toolchain_prefix)/bin/$(build)-g++
default_build_toolchain_AR = $(build_toolchain_prefix)/bin/$(build)-ar
default_build_toolchain_RANLIB = $(build_toolchain_prefix)/bin/$(build)-ranlib
default_build_toolchain_STRIP = $(build_toolchain_prefix)/bin/$(build)-strip
default_build_toolchain_NM = $(build_toolchain_prefix)/bin/$(build)-nm
default_build_toolchain_CFLAGS = -O2
default_build_toolchain_CXXFLAGS = -O2

default_build_CC = $(build_toolchain_prefix)/bin/$(build)-gcc
default_build_CXX = $(build_toolchain_prefix)/bin/$(build)-g++
default_build_AR = $(build_toolchain_prefix)/bin/$(build)-ar
default_build_RANLIB = $(build_toolchain_prefix)/bin/$(build)-ranlib
default_build_STRIP = $(build_toolchain_prefix)/bin/$(build)-strip
default_build_CFLAGS = -O2
default_build_CXXFLAGS = -O2

#default_build_toolchain_CC = $(default_build_CC)
#default_build_toolchain_CXX = $(default_build_CXX)
#default_build_toolchain_AR = $(default_build_AR)
#default_build_toolchain_RANLIB = $(default_build_RANLIB)
#default_build_toolchain_STRIP = $(default_build_STRIP)
#default_build_toolchain_CFLAGS = -O2
#default_build_toolchain_CXXFLAGS = -O2

default_old_build_toolchain_CC = /usr/bin/gcc
default_old_build_toolchain_CXX = /usr/bin/g++
default_old_build_toolchain_AR = /usr/bin/ar
default_old_build_toolchain_RANLIB = /usr/bin/ranlib
default_old_build_toolchain_STRIP = /usr/bin/strip
default_old_build_toolchain_NM = /usr/bin/nm

default_build_SHA256SUM = sha256sum
default_build_OTOOL = $(build)-otool
default_build_DOWNLOAD = curl --location --fail --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) -o
default_build_INSTALL_NAME_TOOL = $(build)-install_name_tool


define add_build_tool_func
build_$(build_os)_$1 ?= $$(default_build_$1)
build_toolchain_$(build_os)_$1 ?= $$(default_build_toolchain_$1)
old_build_toolchain_$(build_os)_$1 ?= $$(default_old_build_toolchain_$1)
build_$(build_arch)_$(build_os)_$1 ?= $$(build_$(build_os)_$1)
build_toolchain_$(build_arch)_$(build_os)_$1 ?= $$(build_toolchain_$(build_os)_$1)
old_build_toolchain_$(build_arch)_$(build_os)_$1 ?= $$(old_build_toolchain_$(build_os)_$1)
build_$1=$$(build_$(build_arch)_$(build_os)_$1)
build_toolchain_$1=$$(build_toolchain_$(build_arch)_$(build_os)_$1)
old_build_toolchain_$1=$$(old_build_toolchain_$(build_arch)_$(build_os)_$1)
endef

$(foreach var,CC CXX AR RANLIB NM STRIP SHA256SUM DOWNLOAD OTOOL INSTALL_NAME_TOOL,$(eval $(call add_build_tool_func,$(var))))
define add_build_flags_func
build_$(build_os)_$1 ?= $$(default_build_$1)
build_toolchain_$(build_os)_$1 ?= $$(default_build_toolchain_$1)
build_$(build_arch)_$(build_os)_$1 ?= $$(build_$(build_os)_$1)
build_toolchain_$(build_arch)_$(build_os)_$1 ?= $$(build_toolchain_$(build_os)_$1)
build_$1=$$(build_$(build_arch)_$(build_os)_$1)
build_toolchain_$1=$$(build_toolchain_$(build_arch)_$(build_os)_$1)
endef

$(foreach flags, CFLAGS CXXFLAGS LDFLAGS, $(eval $(call add_build_flags_func,$(flags))))
