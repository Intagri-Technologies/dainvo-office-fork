# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Dainvo Office Languages dialog (cui/source/dialogs/dainvolanguages.cxx),
# driven headless against linguistic/qa/data/langpacks.

$(eval $(call gb_CppunitTest_CppunitTest,cui_dainvolanguages))

$(eval $(call gb_CppunitTest_add_exception_objects,cui_dainvolanguages, \
    cui/qa/unit/dainvolanguages \
))

$(eval $(call gb_CppunitTest_use_library_objects,cui_dainvolanguages,cui))

$(eval $(call gb_CppunitTest_set_include,cui_dainvolanguages,\
    -I$(SRCDIR)/cui/source/inc \
    -I$(SRCDIR)/cui/inc \
    $$(INCLUDE) \
))

$(eval $(call gb_CppunitTest_add_defs,cui_dainvolanguages,\
    -DCUI_DLLIMPLEMENTATION \
))

$(eval $(call gb_CppunitTest_use_sdk_api,cui_dainvolanguages))

$(eval $(call gb_CppunitTest_use_custom_headers,cui_dainvolanguages,\
	officecfg/registry \
	oox/generated \
))

$(eval $(call gb_CppunitTest_use_libraries,cui_dainvolanguages,\
	$(call gb_Helper_optional,AVMEDIA,avmedia) \
	$(call gb_Helper_optional,SCRIPTING,basctl) \
	basegfx \
	comphelper \
	cppu \
	cppuhelper \
	docmodel \
	drawinglayer \
	editeng \
	fwk \
	i18nlangtag \
	i18nutil \
	$(if $(ENABLE_JAVA),jvmfwk) \
	lng \
	$(call gb_Helper_optional,OPENCL,opencl) \
	sal \
	salhelper \
	sax \
	sb \
	sfx \
	sot \
	svl \
	svt \
	svxcore \
	svx \
	test \
	tk \
	tl \
	ucbhelper \
	unotest \
	utl \
	vcl \
	$(if $(ENABLE_BREAKPAD),crashreport) \
))

$(eval $(call gb_CppunitTest_use_externals,cui_dainvolanguages,\
	boost_headers \
	$(call gb_Helper_optional,OPENCL,clew) \
	icuuc \
	icu_headers \
	libxml2 \
	orcus-parser \
	orcus \
	zxing \
))

$(eval $(call gb_CppunitTest_use_ure,cui_dainvolanguages))

$(eval $(call gb_CppunitTest_use_vcl,cui_dainvolanguages))

$(eval $(call gb_CppunitTest_use_rdb,cui_dainvolanguages,services))

$(eval $(call gb_CppunitTest_use_configuration,cui_dainvolanguages))

# vim: set noet sw=4 ts=4:
