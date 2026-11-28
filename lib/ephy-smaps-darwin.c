/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "ephy-smaps.h"

#include <mach/mach.h>
#include <mach/task_info.h>
#include <unistd.h>

struct _EphySMaps {
  GObject parent_instance;
};

G_DEFINE_FINAL_TYPE (EphySMaps, ephy_smaps, G_TYPE_OBJECT)

static void
ephy_smaps_init (EphySMaps *smaps)
{
}

static void
ephy_smaps_class_init (EphySMapsClass *klass)
{
}

EphySMaps *
ephy_smaps_new (void)
{
  return g_object_new (EPHY_TYPE_SMAPS, NULL);
}

char *
ephy_smaps_to_html (EphySMaps *smaps)
{
  mach_task_basic_info_data_t info = { 0 };
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t result;
  GString *html;

  g_return_val_if_fail (EPHY_IS_SMAPS (smaps), NULL);

  /* Inspect only our own task. Do not require task_for_pid or extra
   * entitlements to inspect the isolated WebKit child processes. */
  result = task_info (mach_task_self (), MACH_TASK_BASIC_INFO,
                      (task_info_t)&info, &count);

  html = g_string_new ("<body><h1>Memory usage (macOS)</h1>");
  g_string_append_printf (html, "<h2>Browser process (PID %d)</h2>", (int)getpid ());

  if (result == KERN_SUCCESS && count >= MACH_TASK_BASIC_INFO_COUNT) {
    g_string_append_printf (html,
                            "<table>"
                            "<tr><th>Resident memory (bytes)</th><td>%" G_GUINT64_FORMAT "</td></tr>"
                            "<tr><th>Virtual address space (bytes)</th><td>%" G_GUINT64_FORMAT "</td></tr>"
                            "</table>",
                            (guint64)info.resident_size,
                            (guint64)info.virtual_size);
  } else {
    g_string_append (html, "<p>Memory information is unavailable.</p>");
  }

  g_string_append (html,
                   "<p>Current browser process only; WebKit child processes and "
                   "Linux smaps breakdowns are not included. Virtual address "
                   "space is not physical memory usage.</p></body>");
  return g_string_free_and_steal (html);
}
