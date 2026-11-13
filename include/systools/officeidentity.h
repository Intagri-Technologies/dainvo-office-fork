/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <config_version.h>

#ifdef DAINVO_VERSION
#define OFFICE_APP_USER_MODEL_PREFIX "IntagriTechnologies.DainvoOffice."
#define OFFICE_PROG_ID_PREFIX "DainvoOffice."
#define OFFICE_REGISTRY_ROOT L"Dainvo"
#else
#define OFFICE_APP_USER_MODEL_PREFIX "TheDocumentFoundation.LibreOffice."
#define OFFICE_PROG_ID_PREFIX "LibreOffice."
#define OFFICE_REGISTRY_ROOT L"LibreOffice"
#endif

#define OFFICE_WIDE_PROG_ID_PREFIX L"" OFFICE_PROG_ID_PREFIX
#define OFFICE_UNO_INSTALL_PATH L"Software\\" OFFICE_REGISTRY_ROOT L"\\UNO\\InstallPath"
