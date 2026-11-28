/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-flatpak-utils.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

gboolean
ephy_desktop_open_uri (const char       *uri,
                       gboolean          reveal,
                       EphyOpenUriFlags  flags)
{
  g_return_val_if_fail (uri != NULL, FALSE);

  /* UI operations must run on the application's main thread. */
  if (![NSThread isMainThread]) {
    g_warning ("Native URI opening must run on the main thread");
    return FALSE;
  }

  @autoreleasepool {
    NSString *value = [NSString stringWithUTF8String:uri];
    NSURL *url = value ? [NSURL URLWithString:value] : nil;
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    BOOL opened;

    if (!url || ![url scheme] || (reveal && ![url isFileURL])) {
      g_warning ("Invalid URI for native desktop integration");
      return FALSE;
    }

    /* Do not silently replace the portal's explicit consent requirement.
     * The native application-selection/consent flow is not implemented yet. */
    if (flags & EPHY_OPEN_URI_FLAGS_REQUIRE_USER_INTERACTION) {
      g_warning ("Native application selection with user consent is not implemented on macOS");
      return FALSE;
    }

    if (reveal)
      opened = [workspace selectFile:[url path] inFileViewerRootedAtPath:@""];
    else
      opened = [workspace openURL:url];

    if (!opened)
      g_warning ("Native desktop service could not open the requested URI");

    return opened ? TRUE : FALSE;
  }
}

void
ephy_open_uri_via_flatpak_portal (const char       *uri,
                                  EphyOpenUriFlags  flags)
{
  ephy_desktop_open_uri (uri, FALSE, flags);
}

void
ephy_open_directory_via_flatpak_portal (const char       *uri,
                                        EphyOpenUriFlags  flags)
{
  ephy_desktop_open_uri (uri, TRUE, flags);
}

gboolean
ephy_can_install_web_apps (void)
{
  /* No native launcher implementation yet; never advertise portal support. */
  return FALSE;
}
