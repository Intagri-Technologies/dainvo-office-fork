# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_ExternalPackage_ExternalPackage,winsparkle,winsparkle))

winsparkle_ARCH := $(if $(filter X86_64,$(CPUNAME)),x64,$(if $(filter AARCH64,$(CPUNAME)),ARM64,Win32))

# sfx2 loads it at run time (sfx2/source/appl/dainvoupdate.cxx).
$(eval $(call gb_ExternalPackage_add_unpacked_file,winsparkle,$(LIBO_BIN_FOLDER)/WinSparkle.dll,WinSparkle-$(WINSPARKLE_VERSION)/$(winsparkle_ARCH)/Release/WinSparkle.dll))
$(eval $(call gb_ExternalPackage_add_unpacked_file,winsparkle,$(LIBO_SHARE_FOLDER)/licenses/WinSparkle/COPYING,WinSparkle-$(WINSPARKLE_VERSION)/COPYING))
$(eval $(call gb_ExternalPackage_add_unpacked_file,winsparkle,$(LIBO_SHARE_FOLDER)/licenses/WinSparkle/COPYING.expat,WinSparkle-$(WINSPARKLE_VERSION)/COPYING.expat))

# vim: set noet sw=4 ts=4:
