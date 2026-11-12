# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
# This file is part of the LibreOffice project.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. See http://mozilla.org/MPL/2.0/.

$(eval $(call gb_Executable_Executable,sfx2_metadatastall))
$(eval $(call gb_Executable_add_exception_objects,sfx2_metadatastall,sfx2/qa/cppunit/metadatastall))
$(eval $(call gb_Executable_use_libraries,sfx2_metadatastall,sal))
