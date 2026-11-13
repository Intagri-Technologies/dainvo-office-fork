/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ustring.hxx>

namespace formula
{
inline OUString rebrandVendorFunction(const OUString& rName)
{
    constexpr std::u16string_view aOldPrefix = u"ORG.LIBREOFFICE.";
    constexpr std::u16string_view aNewPrefix = u"ORG.DAINVO.";
    const sal_Int32 nOffset = rName.startsWithIgnoreAsciiCase(u"_xlfn.") ? 6 : 0;
    if (!rName.matchIgnoreAsciiCase(aOldPrefix, nOffset))
        return rName;
    return rName.replaceAt(nOffset, aOldPrefix.size(), aNewPrefix);
}
}
