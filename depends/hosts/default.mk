default_host_CC = $(host_toolchain_prefix)/bin/$(canonical_host)-gcc
default_host_CXX = $(host_toolchain_prefix)/bin/$(canonical_host)-g++
default_host_AR = $(host_toolchain_prefix)/bin/$(canonical_host)-ar
default_host_RANLIB = $(host_toolchain_prefix)/bin/$(canonical_host)-ranlib
default_host_STRIP = $(host_toolchain_prefix)/bin/$(canonical_host)-strip
default_host_LIBTOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-libtool
default_host_INSTALL_NAME_TOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-install_name_tool
default_host_OTOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-otool
default_host_NM = $(host_toolchain_prefix)/bin/$(canonical_host)-nm
default_host_WINDRES = $(host_toolchain_prefix)/bin/$(canonical_host)-windres
default_host_WINDMC = $(host_toolchain_prefix)/bin/$(canonical_host)-windmc

default_host_toolchain_CC = $(host_toolchain_prefix)/bin/$(canonical_host)-gcc
default_host_toolchain_CXX = $(host_toolchain_prefix)/bin/$(canonical_host)-g++
default_host_toolchain_AR = $(host_toolchain_prefix)/bin/$(canonical_host)-ar
default_host_toolchain_RANLIB = $(host_toolchain_prefix)/bin/$(canonical_host)-ranlib
default_host_toolchain_STRIP = $(host_toolchain_prefix)/bin/$(canonical_host)-strip
default_host_toolchain_LIBTOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-libtool
default_host_toolchain_INSTALL_NAME_TOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-install_name_tool
default_host_toolchain_OTOOL = $(host_toolchain_prefix)/bin/$(canonical_host)-otool
default_host_toolchain_NM = $(host_toolchain_prefix)/bin/$(canonical_host)-nm
default_host_toolchain_WINDRES = $(host_toolchain_prefix)/bin/$(canonical_host)-windres
default_host_toolchain_WINDMC = $(host_toolchain_prefix)/bin/$(canonical_host)-windmc

define add_host_tool_func
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1?=$$($(host_os)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)

host_toolchain_$(host_os)_$1 ?= $$(default_host_toolchain_$1)
host_toolchain_$(host_arch)_$(host_os)_$1 ?= $$(host_toolchain_$(host_os)_$1)
host_toolchain_$1=$$(host_toolchain_$(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1)
$(host_arch)_$(host_os)_$(release_type)_$1 += $($(host_os)_$(release_type)_$1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
host_$(release_type)_$1 = $$($(host_arch)_$(host_os)_$(release_type)_$1)
endef

$(foreach tool,CC CXX AR RANLIB STRIP NM LIBTOOL OTOOL INSTALL_NAME_TOOL WINDRES WINDMC,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,CFLAGS CXXFLAGS CPPFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))
