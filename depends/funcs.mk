delete_built=1
delete_sources=1
tar_staged=1

define int_vars
#Set defaults for vars which may be overridden per-package
$(1)_cc=$($($1_type)_CC)
$(1)_cxx=$($($1_type)_CXX)
$(1)_objc=$($($1_type)_OBJC)
$(1)_objcxx=$($($1_type)_OBJCXX)
$(1)_ar=$($($1_type)_AR)
$(1)_ranlib=$($($1_type)_RANLIB)
$(1)_libtool=$($($1_type)_LIBTOOL)
$(1)_cflags=$($($1_type)_CFLAGS)
$(1)_cxxflags=$($($1_type)_CXXFLAGS)
$(1)_ldflags=$($($1_type)_LDFLAGS)
$(1)_dependency_targets=
$(1)_patch_dir:=$(PATCHES_PATH)/$1
$(1)_cppflags:=-I$($($1_type)_prefix)/include
$(1)_ldflags=-L$($($1_type)_prefix)/lib
endef

define int_get_all_dependencies
$(sort $(foreach dep,$2,$2 $(call int_get_all_dependencies,$1,$($(dep)_dependencies))))
endef

define fetch_file
(test -f $(SOURCES_PATH)/$3 || ( mkdir -p $$($(1)_extract_dir) && $(build_DOWNLOAD) "$$($(1)_extract_dir)/$3.temp" "$2" && echo "$4  $$($(1)_extract_dir)/$3.temp" | $(build_SHA256SUM) -c && mv $$($(1)_extract_dir)/$3.temp $(SOURCES_PATH)/$3 ))
endef

define int_get_build_id
$(eval $1_dependencies += $($(1)_$(host_arch)_$(host_os)_dependencies) $($(1)_$(host_os)_dependencies))
$(eval $1_all_dependencies:=$(call int_get_all_dependencies,$1,$($($1_type)_native_toolchain) $($1_dependencies)))
$(foreach dep,$($1_all_dependencies),$(eval $(1)_build_id_deps+=$(dep)-$($(dep)_version)-$($(dep)_revision)))
$(eval $(1)_build_id_long:=base-$(base_revision)_os-$($(host_arch)_$(host_os)_revision) $1-$($1_version)-$($(1)_revision) $($1_build_id_deps))
$(eval $(1)_build_id:=$(shell echo -n "$($(1)_build_id_long)" | $(build_SHASUM) | cut -c-5))
final_build_id_long+=$($(package)_build_id_long)

#compute package-specific paths
$(1)_build_subdir?=.
$(1)_download_file?=$($(1)_file_name)
$(1)_source:=$(SOURCES_PATH)/$($(1)_file_name)
$(1)_staging_dir=$(base_staging_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_revision)-$($1_build_id)
$(1)_destdir:=$(base_staging_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_revision)-$($1_build_id)
$(1)_staging_prefix_dir:=$$($(1)_staging_dir)$($($1_type)_prefix)
$(1)_extract_dir:=$(base_build_dir)/$(host)/$(1)/$($(1)_version)-$($(1)_revision)-$($1_build_id)
$(1)_build_dir:=$$($(1)_extract_dir)/$$($(1)_build_subdir)
$(1)_prefixbin:=$($($1_type)_prefix)/bin/
$(1)_cached:=$(BASE_CACHE)/$(host)/$1/$(1)-$($(1)_version)-$($(1)_revision)-$($1_build_id).tar.gz

#stamps
$(1)_fetched=$$($(1)_extract_dir)/.stamp_fetched
$(1)_extracted=$$($(1)_extract_dir)/.stamp_extracted
$(1)_preprocessed=$$($(1)_extract_dir)/.stamp_preprocessed
$(1)_cleaned=$$($(1)_extract_dir)/.stamp_cleaned
$(1)_built=$$($(1)_build_dir)/.stamp_built
$(1)_configured=$$($(1)_build_dir)/.stamp_configured
$(1)_staged=$$($(1)_staging_dir)/.stamp_staged
$(1)_postprocessed=$$($(1)_staging_prefix_dir)/.stamp_postprocessed
$(1)_download_path_fixed=$(subst :,\:,$$($(1)_download_path))


#default commands
$(1)_fetch_cmds ?= $(call fetch_file,$1,$(subst \:,:,$$($(1)_download_path_fixed)/$$($(1)_download_file)),$($1_file_name),$($(1)_sha256_hash))
$(1)_extract_cmds ?= echo "$$($(1)_sha256_hash)  $$($(1)_source)" | $(build_SHA256SUM) -c && tar --strip-components=1 -xf $$($(1)_source)
$(1)_preprocess_cmds ?=
$(1)_build_cmds ?=
$(1)_config_cmds ?=
$(1)_stage_cmds ?=
$(1)_tar_cmds ?= tar czf $$($(1)_cached).temp * && mv $$($(1)_cached).temp $$($(1)_cached)
$(1)_set_vars ?=

$(foreach dep_target,$($(1)_all_dependencies),$(eval $(1)_dependency_targets=$($(dep_target)_cached)))
endef


define int_config_attach_build_config
$(eval $(call $(1)_set_vars,$(1)))
$(1)_cflags+=$($(1)_cflags_$(host_arch))
$(1)_cflags+=$($(1)_cflags_$(host_os))
$(1)_cflags+=$($(1)_cflags_$(host_arch)_$(host_os))

$(1)_cxxflags+=$($(1)_cxxflags_$(host_arch))
$(1)_cxxflags+=$($(1)_cxxflags_$(host_os))
$(1)_cxxflags+=$($(1)_cxxflags_$(host_arch)_$(host_os))

$(1)_cppflags+=$($(1)_cppflags_$(host_arch))
$(1)_cppflags+=$($(1)_cppflags_$(host_os))
$(1)_cppflags+=$($(1)_cppflags_$(host_arch)_$(host_os))

$(1)_ldflags+=$($(1)_ldflags_$(host_arch))
$(1)_ldflags+=$($(1)_ldflags_$(host_os))
$(1)_ldflags+=$($(1)_ldflags_$(host_arch)_$(host_os))

$(1)_build_opts+=$$($(1)_build_opts_$(host_arch))
$(1)_build_opts+=$$($(1)_build_opts_$(host_os))
$(1)_build_opts+=$$($(1)_build_opts_$(host_arch)_$(host_os))

$(1)_config_opts+=$$($(1)_config_opts_$(host_arch))
$(1)_config_opts+=$$($(1)_config_opts_$(host_os))
$(1)_config_opts+=$$($(1)_config_opts_$(host_arch)_$(host_os))

$(1)_config_env+=$($(1)_config_env_$(host_arch))
$(1)_config_env+=$($(1)_config_env_$(host_os))
$(1)_config_env+=$($(1)_config_env_$(host_arch)_$(host_os))

$(1)_config_env+=PKG_CONFIG_LIBDIR=$($($1_type)_prefix)/lib/pkgconfig
$(1)_config_env+=PKG_CONFIG_PATH=$($($1_type)_prefix)/share/pkgconfig
$(1)_config_env+=PATH=$(build_prefix)/bin:$(PATH)
$(1)_build_env+=PATH=$(build_prefix)/bin:$(PATH)
$(1)_stage_env+=PATH=$(build_prefix)/bin:$(PATH)
$(1)_autoconf=./configure --host=$($($1_type)_host) --prefix=$($($1_type)_prefix) $$($1_config_opts) CC="$$($1_cc)" CXX="$$($1_cxx)" AR="$$($1_ar)" RANLIB="$$($1_ranlib)" CPPFLAGS="$$($1_cppflags)" LDFLAGS="$$($1_ldflags)"
ifneq ($($1_cflags),)
$(1)_autoconf += CFLAGS="$$($1_cflags)"
endif
ifneq ($($1_cxxflags),)
$(1)_autoconf += CXXFLAGS="$$($1_cxxflags)"
endif
endef

define int_add_cmds
$($(1)_fetched): | $($(1)_dependency_targets)
	@mkdir -p $$(@D) $(SOURCES_PATH)
	@echo Fetching $1...
	@cd $$(@D); $(call $(1)_fetch_cmds,$1)
	@touch $$@
$($(1)_extracted): | $($(1)_fetched)
	@mkdir -p $$(@D)
	@echo Extracting $1...
	@cd $$(@D); $(call $(1)_extract_cmds,$1)
	@touch $$@
$($(1)_preprocessed): | $($(1)_dependencies) $($(1)_extracted)
	@mkdir -p $$(@D)
	@echo Preprocessing $1...
	@cd $$(@D); $(call $(1)_preprocess_cmds, $1)
	@touch $$@
$($(1)_configured): | $($(1)_preprocessed)
	@rm -rf $(host_prefix); mkdir -p $(host_prefix); cd $(host_prefix); $(foreach package,$($(1)_all_dependencies), tar xf $($(package)_cached); )
	@mkdir -p $$(@D)
	@echo Configuring $1...
	@+cd $$(@D); $($(1)_config_env) $(call $(1)_config_cmds, $1)
	@touch $$@
$($(1)_built): | $($(1)_configured)
	@mkdir -p $$(@D)
	@echo Building $1...
	@+cd $$(@D); $($(1)_build_env) $(call $(1)_build_cmds, $1)
	@touch $$@
$($(1)_staged): | $($(1)_built)
	@echo Staging $1...
	@mkdir -p $($(1)_staging_dir)/$(host_prefix)
	@cd $($(1)_build_dir); $($(1)_stage_env) $(call $(1)_stage_cmds, $1)
	@rm -rf $($(1)_extract_dir)
	@touch $$@
$($(1)_postprocessed): | $($(1)_staged)
	@echo Postprocessing $1...
	@cd $($(1)_staging_prefix_dir); $(call $(1)_postprocess_cmds)
	@touch $$@
$($(1)_cached): | $($(1)_dependencies) $($(1)_postprocessed)
	@echo Caching $1...
	cd $$($(1)_staging_dir)/$(host_prefix); find . | sort | tar --no-recursion -czf $$($(1)_staging_dir)/$$(@F) -T -
	@mkdir -p $$(@D)
	@rm -rf $$(@D) && mkdir -p $$(@D)
	@mv $$($(1)_staging_dir)/$$(@F) $$(@)
	@rm -rf $($(1)_staging_dir)

$(1): | $($(1)_cached)
.SECONDARY: $($(1)_postprocessed) $($(1)_staged) $($(1)_built) $($(1)_configured) $($(1)_preprocessed) $($(1)_extracted) $($(1)_fetched)

endef

$(host_arch)_$(host_os)_native_toolchain?=$($(host_os)_native_toolchain)
packages += $($(host_arch)_$(host_os)_packages) $($(host_os)_packages)
native_packages += $($(host_arch)_$(host_os)_packages_for_build) $($(host_os)_packages_for_build)

#set the type for host/build packages.
$(foreach native_package,$(native_packages),$(eval $(native_package)_type=build))
$(foreach package,$(packages),$(eval $(package)_type=$(host_arch)_$(host_os)))

#set overridable defaults
$(foreach package,$(packages) $(native_packages),$(eval $(call int_vars,$(package))))

#include package files
$(foreach package,$(packages) $(native_packages),$(eval include packages/$(package).mk))

#compute per-package vars before reading package vars
$(foreach package,$(packages) $(native_packages),$(eval $(call int_get_build_id,$(package))))

#compute final vars after reading package vars
$(foreach package,$(packages) $(native_packages),$(eval $(call int_config_attach_build_config,$(package))))

#create build targets
$(foreach package,$(packages) $(native_packages),$(eval $(call int_add_cmds,$(package))))

$(foreach package,$(packages),$(eval $($(package)_fetched): |$($($(host_arch)_$(host_os)_native_toolchain)_cached) ))
$(foreach package,$(packages),$(eval cached: | $($(package)_cached)))
