# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Module_Module,extras))

$(eval $(call gb_Module_add_targets,extras,\
	CustomTarget_autocorr \
	CustomTarget_autotextuser \
	CustomTarget_glade \
	CustomTarget_gallsystem \
	CustomTarget_templates \
	Package_autocorr \
	Package_autotext \
	Package_autotextuser \
	Package_cfgsrvnolang \
	Package_cfgusr \
	Package_database \
	Package_databasebiblio \
	Package_fonts \
	Package_resource_fonts \
	Package_gallbullets \
	Package_gallmytheme \
	Package_gallroot \
	Package_gallsystem \
	Package_gallsystemstr \
	Package_glade \
	Package_labels \
	$(if $(filter WNT,$(OS)),Package_newfiles) \
	Package_palettes \
	Package_tplpresnt \
	Package_tplwizdesktop \
	Package_wordbook \
))

$(eval $(call gb_Module_add_l10n_targets,extras,\
	CustomTarget_autotextshare \
	AllLangPackage_autotextshare \
))

# Dainvo: no Gallery_sound. The bundled slide transition sounds are dated;
# users can still pick their own sound files, and PowerPoint imports keep
# their embedded sounds.
ifneq ($(WITH_GALLERY_BUILD),)
$(eval $(call gb_Module_add_targets,extras,\
	Gallery_backgrounds \
	Gallery_phosphor-office \
	Gallery_phosphor-business \
	Gallery_phosphor-arrows \
	Gallery_phosphor-communication \
	Gallery_phosphor-people \
	Gallery_phosphor-technology \
	Gallery_phosphor-design \
	Gallery_phosphor-media \
	Gallery_phosphor-travel \
	Gallery_phosphor-nature \
	Gallery_phosphor-signs \
	Gallery_phosphor-leisure \
	Package_gallphosphor \
))
endif

ifneq ($(WITH_TEMPLATES),)
$(eval $(call gb_Module_add_targets,extras,\
	Package_templates \
	Package_tplwizard \
	Package_tplwizbitmap \
))
endif

$(eval $(call gb_Module_add_targets,extras,\
	Personas \
))

$(eval $(call gb_Module_add_targets,extras,\
	CustomTarget_opensymbol \
))

# vim: set noet sw=4 ts=4:
