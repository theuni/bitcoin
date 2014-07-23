default_host_CC = gcc
default_host_CXX = g++
default_host_AR = ar
default_host_RANLIB = ranlib
default_host_STRIP = strip
default_host_INSTALL_NAME_TOOL = install_name_tool
default_host_OTOOL = otool

#TODO: fix
define add_host_tool_func
prefixed_host_$1 = $(host_toolchain)$(default_host_$1)
ifneq ($$(prefixed_host_$1),)
default_host_$1=$$(prefixed_host_$1)
endif
$(host_os)_$1?=$$(default_host_$1)
$(host_arch)_$(host_os)_$1?=$$($(host_os)_$1)
ifeq "$(origin $1)" "command line"
$(host_arch)_$(host_os)_$1=$($1)
endif
host_$1=$$($(host_arch)_$(host_os)_$1)
endef

define add_host_flags_func
$(host_arch)_$(host_os)_$1 += $($(host_os)_$1) $($1)
host_$1 = $$($(host_arch)_$(host_os)_$1)
endef

$(foreach tool,CC CXX AR RANLIB STRIP LIBTOOL OTOOL INSTALL_NAME_TOOL,$(eval $(call add_host_tool_func,$(tool))))
$(foreach flags,CFLAGS CXXFLAGS LDFLAGS, $(eval $(call add_host_flags_func,$(flags))))
