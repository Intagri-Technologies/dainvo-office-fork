# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Package_Package,dainvo_fonts,$(SRCDIR)/dainvo/fonts))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Intos-Bold.ttf,intos/Intos-Bold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Intos-BoldItalic.ttf,intos/Intos-BoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Intos-Italic.ttf,intos/Intos-Italic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Intos-Regular.ttf,intos/Intos-Regular.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosDisplay-Bold.ttf,intos/IntosDisplay-Bold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosDisplay-BoldItalic.ttf,intos/IntosDisplay-BoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosDisplay-Italic.ttf,intos/IntosDisplay-Italic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosDisplay-Regular.ttf,intos/IntosDisplay-Regular.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosNarrow-Bold.ttf,intos/IntosNarrow-Bold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosNarrow-BoldItalic.ttf,intos/IntosNarrow-BoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosNarrow-Italic.ttf,intos/IntosNarrow-Italic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosNarrow-Regular.ttf,intos/IntosNarrow-Regular.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosSerif-Bold.ttf,intos/IntosSerif-Bold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosSerif-BoldItalic.ttf,intos/IntosSerif-BoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosSerif-Italic.ttf,intos/IntosSerif-Italic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/IntosSerif-Regular.ttf,intos/IntosSerif-Regular.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/licenses/intos/LICENSE.txt,intos/LICENSE.txt))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-Bold.ttf,gelasio/Gelasio-Bold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-BoldItalic.ttf,gelasio/Gelasio-BoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-Italic.ttf,gelasio/Gelasio-Italic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-Medium.ttf,gelasio/Gelasio-Medium.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-MediumItalic.ttf,gelasio/Gelasio-MediumItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-Regular.ttf,gelasio/Gelasio-Regular.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-SemiBold.ttf,gelasio/Gelasio-SemiBold.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/truetype/Gelasio-SemiBoldItalic.ttf,gelasio/Gelasio-SemiBoldItalic.ttf))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/licenses/gelasio/OFL.txt,gelasio/OFL.txt))
$(eval $(call gb_Package_add_file,dainvo_fonts,$(LIBO_SHARE_FOLDER)/fonts/licenses/manifest.json,manifest.json))
