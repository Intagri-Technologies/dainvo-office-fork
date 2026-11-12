# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
# This file is part of the LibreOffice project.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. See http://mozilla.org/MPL/2.0/.

$(eval $(call gb_Executable_Executable,recentfilemetadata))
$(eval $(call gb_Executable_set_include,recentfilemetadata,\
    $$(INCLUDE) -I$(SRCDIR)/sfx2/inc \
))
$(eval $(call gb_Executable_add_exception_objects,recentfilemetadata,\
    sfx2/source/control/recentfilemetadata \
))
$(eval $(call gb_Executable_use_libraries,recentfilemetadata,sal))
$(eval $(call gb_Executable_add_default_nativeres,recentfilemetadata))
