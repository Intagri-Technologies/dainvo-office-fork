/* -*- Mode: ObjC; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Sparkle 2 for Dainvo Office on macOS. Sparkle.framework is loaded at run
// time from Contents/Frameworks, so there is no link-time dependency and a
// build without it falls back to the built-in check (dainvoupdate.cxx).
// Sparkle reads SUFeedURL, SUPublicEDKey and SUScheduledCheckInterval from
// Info.plist; `defaults write com.dainvo.office SUFeedURL <url>` overrides
// the feed for testing.

#include <sal/config.h>

#include <config_version.h>

#ifdef ENABLE_DAINVO_UPDATER

#include <sal/log.hxx>

#include <premac.h>
#import <Cocoa/Cocoa.h>
#include <postmac.h>

// The parts of SPUStandardUpdaterController we use, so we need no Sparkle headers.
@protocol DainvoSparkleController
- (instancetype)initWithStartingUpdater:(BOOL)startUpdater
                        updaterDelegate:(id)updaterDelegate
                     userDriverDelegate:(id)userDriverDelegate;
- (void)checkForUpdates:(id)sender;
@end

namespace
{
id<DainvoSparkleController> g_pController = nil;
}

bool DainvoSparkleStart()
{
    if (g_pController)
        return true;
    @autoreleasepool
    {
        NSString* pPath = [[[NSBundle mainBundle] privateFrameworksPath]
            stringByAppendingPathComponent:@"Sparkle.framework"];
        NSBundle* pBundle = [NSBundle bundleWithPath:pPath];
        NSError* pError = nil;
        if (!pBundle || ![pBundle loadAndReturnError:&pError])
        {
            SAL_INFO("sfx.appl", "Could not load Sparkle.framework: "
                                     << (pError ? [[pError description] UTF8String] : "missing"));
            return false;
        }
        Class pClass = NSClassFromString(@"SPUStandardUpdaterController");
        if (!pClass)
            return false;
        // Retained for the rest of the session. Starting the updater schedules
        // the automatic checks; Sparkle asks the user on the second launch
        // whether to check automatically.
        g_pController = [static_cast<id<DainvoSparkleController>>([pClass alloc])
            initWithStartingUpdater:YES
                    updaterDelegate:nil
                 userDriverDelegate:nil];
        return g_pController != nil;
    }
}

bool DainvoSparkleCheckNow()
{
    if (!g_pController)
        return false;
    [g_pController checkForUpdates:nil];
    return true;
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
