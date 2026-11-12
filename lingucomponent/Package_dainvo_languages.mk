# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Dainvo Office: language packs bundled with the app (US English spelling,
# US and UK English hyphenation), in the same layout as downloaded packs.
# See dainvo/docs/LANGUAGE_TOOLS.md. pack.json comes from dainvo/languages,
# the data from the dictionaries submodule.

dainvo_languages_DIR := $(LIBO_SHARE_FOLDER)/languages
dainvo_languages_EN_US_SPELL := $(dainvo_languages_DIR)/en-US/spell-2025.04.01
dainvo_languages_EN_US_HYPH := $(dainvo_languages_DIR)/en-US/hyph-2025.04.01
dainvo_languages_EN_GB_HYPH := $(dainvo_languages_DIR)/en-GB/hyph-2025.04.01

$(eval $(call gb_Package_Package,dainvo_languages,$(SRCDIR)))

$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_SPELL)/pack.json,dainvo/languages/bundled/en-US/spell-2025.04.01/pack.json))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_SPELL)/en_US.aff,dictionaries/en/en_US.aff))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_SPELL)/en_US.dic,dictionaries/en/en_US.dic))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_SPELL)/licenses/README_en_US.txt,dictionaries/en/README_en_US.txt))

$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_HYPH)/pack.json,dainvo/languages/bundled/en-US/hyph-2025.04.01/pack.json))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_HYPH)/hyph_en_US.dic,dictionaries/en/hyph_en_US.dic))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_US_HYPH)/licenses/README_hyph_en_US.txt,dictionaries/en/README_hyph_en_US.txt))

$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_GB_HYPH)/pack.json,dainvo/languages/bundled/en-GB/hyph-2025.04.01/pack.json))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_GB_HYPH)/hyph_en_GB.dic,dictionaries/en/hyph_en_GB.dic))
$(eval $(call gb_Package_add_file,dainvo_languages,$(dainvo_languages_EN_GB_HYPH)/licenses/README_hyph_en_GB.txt,dictionaries/en/README_hyph_en_GB.txt))

# vim: set noet sw=4 ts=4:
