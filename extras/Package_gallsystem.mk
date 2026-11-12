# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# Dainvo: the old "icons" clip art is replaced by the Phosphor themes (Gallery_phosphor_*)
# Dainvo: the Phosphor icon themes replace the classic Arrows, Diagrams,
# Flowchart and Shapes clip art (and BPMN, Network). Bullets (image bullets),
# fontwork (Fontwork gallery) and symbolshapes (custom shape defaults) are used
# by the app itself.
system_galleries := bullets fontwork symbolshapes

$(eval $(call gb_Package_Package,extras_gallsystem,$(SRCDIR)/extras/source/gallery/gallery_system))

$(eval $(call gb_Package_add_files,extras_gallsystem,$(LIBO_SHARE_FOLDER)/gallery,\
    $(addsuffix .sdg,$(system_galleries)) \
    $(addsuffix .sdv,$(system_galleries)) \
    $(addsuffix .thm,$(system_galleries)) \
))

# vim: set noet sw=4 ts=4:
