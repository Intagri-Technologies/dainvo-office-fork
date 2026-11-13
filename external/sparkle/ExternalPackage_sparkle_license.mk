# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

$(eval $(call gb_ExternalPackage_ExternalPackage,sparkle_license,sparkle))

$(eval $(call gb_ExternalPackage_add_unpacked_file,sparkle_license,$(LIBO_SHARE_FOLDER)/licenses/Sparkle/LICENSE,LICENSE))
