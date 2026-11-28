/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ephy-activex.h"

#include <stddef.h>
#include <string.h>

static unsigned char
ascii_lower (unsigned char c)
{
  if (c >= 'A' && c <= 'Z')
    return c + ('a' - 'A');
  return c;
}

static int
ascii_space (unsigned char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int
ascii_hex (unsigned char c)
{
  c = ascii_lower (c);
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

EphyActiveXControl
ephy_activex_classify (const char *classid)
{
  char guid[37];
  size_t length;

  if (!classid)
    return EPHY_ACTIVEX_NOT_DECLARED;

  while (ascii_space (*classid))
    classid++;

  length = strlen (classid);
  while (length > 0 && ascii_space (classid[length - 1]))
    length--;

  if (length < 6 ||
      ascii_lower (classid[0]) != 'c' ||
      ascii_lower (classid[1]) != 'l' ||
      ascii_lower (classid[2]) != 's' ||
      ascii_lower (classid[3]) != 'i' ||
      ascii_lower (classid[4]) != 'd' || classid[5] != ':')
    return EPHY_ACTIVEX_NOT_DECLARED;

  classid += 6;
  length -= 6;
  if (length == 38 && classid[0] == '{' && classid[37] == '}') {
    classid++;
    length -= 2;
  }

  if (length != 36)
    return EPHY_ACTIVEX_INVALID;

  for (size_t i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (classid[i] != '-')
        return EPHY_ACTIVEX_INVALID;
    } else if (!ascii_hex (classid[i])) {
      return EPHY_ACTIVEX_INVALID;
    }
    guid[i] = ascii_lower (classid[i]);
  }
  guid[36] = '\0';

  if (strcmp (guid, "d27cdb6e-ae6d-11cf-96b8-444553540000") == 0)
    return EPHY_ACTIVEX_FLASH;
  if (strcmp (guid, "6bf52a52-394a-11d3-b153-00c04f79faa6") == 0)
    return EPHY_ACTIVEX_WINDOWS_MEDIA;
  if (strcmp (guid, "02bf25d5-8c17-4b23-bc80-d3488abddc6b") == 0)
    return EPHY_ACTIVEX_QUICKTIME;

  return EPHY_ACTIVEX_UNKNOWN;
}

EphyActiveXFallback
ephy_activex_get_fallback (EphyActiveXControl control)
{
  switch (control) {
    case EPHY_ACTIVEX_FLASH:
      return EPHY_ACTIVEX_FALLBACK_RUFFLE;
    case EPHY_ACTIVEX_WINDOWS_MEDIA:
    case EPHY_ACTIVEX_QUICKTIME:
      return EPHY_ACTIVEX_FALLBACK_NATIVE_MEDIA;
    case EPHY_ACTIVEX_NOT_DECLARED:
    case EPHY_ACTIVEX_INVALID:
    case EPHY_ACTIVEX_UNKNOWN:
    default:
      return EPHY_ACTIVEX_FALLBACK_NONE;
  }
}
