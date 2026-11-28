/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-activex.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

int
main (void)
{
  static const struct {
    const char *classid;
    EphyActiveXControl control;
    EphyActiveXFallback fallback;
  } cases[] = {
    { NULL, EPHY_ACTIVEX_NOT_DECLARED, EPHY_ACTIVEX_FALLBACK_NONE },
    { "", EPHY_ACTIVEX_NOT_DECLARED, EPHY_ACTIVEX_FALLBACK_NONE },
    { "java:Applet.class", EPHY_ACTIVEX_NOT_DECLARED, EPHY_ACTIVEX_FALLBACK_NONE },
    { "CLSID:D27CDB6E-AE6D-11CF-96B8-444553540000", EPHY_ACTIVEX_FLASH, EPHY_ACTIVEX_FALLBACK_RUFFLE },
    { " \tclsid:{d27cdb6e-ae6d-11cf-96b8-444553540000}\r\n", EPHY_ACTIVEX_FLASH, EPHY_ACTIVEX_FALLBACK_RUFFLE },
    { "clsid:6BF52A52-394A-11D3-B153-00C04F79FAA6", EPHY_ACTIVEX_WINDOWS_MEDIA, EPHY_ACTIVEX_FALLBACK_NATIVE_MEDIA },
    { "clsid:02bf25d5-8c17-4b23-bc80-d3488abddc6b", EPHY_ACTIVEX_QUICKTIME, EPHY_ACTIVEX_FALLBACK_NATIVE_MEDIA },
    { "clsid:00000000-0000-0000-0000-000000000000", EPHY_ACTIVEX_UNKNOWN, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e-ae6d-11cf-96b8-44455354000", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e-ae6d-11cf-96b8-444553540000x", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e-ae6d-11cf-96b8-44455354000z", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e_ae6d-11cf-96b8-444553540000", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:{d27cdb6e-ae6d-11cf-96b8-444553540000", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e-ae6d-11cf-96b8-444553540000;version=1", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
    { "clsid:d27cdb6e-ae6d-11cf-96b8-444553540000\nextra", EPHY_ACTIVEX_INVALID, EPHY_ACTIVEX_FALLBACK_NONE },
  };

  for (size_t i = 0; i < sizeof (cases) / sizeof (cases[0]); i++) {
    EphyActiveXControl control = ephy_activex_classify (cases[i].classid);

    assert (control == cases[i].control);
    assert (ephy_activex_get_fallback (control) == cases[i].fallback);
  }

  assert (ephy_activex_get_fallback ((EphyActiveXControl)999) == EPHY_ACTIVEX_FALLBACK_NONE);
  puts ("ActiveX classification checks passed");
  return 0;
}
