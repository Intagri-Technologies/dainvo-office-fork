/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>

#include <memory>

namespace sw
{
// Mockup of UnoCursorPointer because it’s probably difficult to include the header
class UnoCursorPointer
{
public:
    int& operator*() { return *m_pInteger; };
    int* get() { return m_pInteger; };

private:
    int* m_pInteger;
};
}

int* function1(std::unique_ptr<int>& x)
{
    return &*x; // expected-error-re {{'*' followed by '&' operating on '{{.*}}unique_ptr{{.*}}', rather use '.get()' [loplugin:smartpointerderefaddress]}}
}

int* function2(std::shared_ptr<int>& x)
{
    return &*x; // expected-error-re {{'*' followed by '&' operating on '{{.*}}shared_ptr{{.*}}', rather use '.get()' [loplugin:smartpointerderefaddress]}}
}

int* function3(sw::UnoCursorPointer& x)
{
    return &*x; // expected-error-re {{'*' followed by '&' operating on '{{.*}}UnoCursorPointer{{.*}}', rather use '.get()' [loplugin:smartpointerderefaddress]}}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
