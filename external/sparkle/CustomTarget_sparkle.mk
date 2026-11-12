# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Copy Sparkle.framework into instdir with ditto, which keeps the framework's
# symlinks and code signatures intact (gb_Package would flatten them).
# installer::simplepackage copies it into the packaged app the same way.
$(eval $(call gb_CustomTarget_CustomTarget,external/sparkle))

sparkle_FRAMEWORK := $(INSTROOT)/$(LIBO_LIB_FOLDER)/Sparkle.framework

$(call gb_CustomTarget_get_target,external/sparkle) : \
	$(call gb_UnpackedTarball_get_target,sparkle)
	$(call gb_Output_announce,Sparkle.framework,$(true),CPY,1)
	rm -rf "$(sparkle_FRAMEWORK)"
	mkdir -p "$(dir $(sparkle_FRAMEWORK))"
	ditto "$(gb_UnpackedTarball_workdir)/sparkle/Sparkle.framework" "$(sparkle_FRAMEWORK)"
	touch $@

# vim: set noet sw=4 ts=4:
