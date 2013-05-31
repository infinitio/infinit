#!/usr/bin/env python
"""
Generate the C api for metrics.

"""

prototypes = [
  ('metrics_connect_google_attempt', ),
  ('metrics_connect_google_succeed', ),
  ('metrics_connect_google_fail', ),
  ('metrics_connect_facebook_attempt', ),
  ('metrics_connect_facebook_succeed', ),
  ('metrics_connect_facebook_fail', ),
  ('metrics_login_google_attempt', ),
  ('metrics_login_google_succeed', ),
  ('metrics_login_google_fail', ),
  ('metrics_login_facebook_attempt', ),
  ('metrics_login_facebook_succeed', ),
  ('metrics_login_facebook_fail', ),
  ('metrics_import_google_attempt', ),
  ('metrics_import_google_succeed', ),
  ('metrics_import_google_fail', ),
  ('metrics_import_facebook_attempt', ),
  ('metrics_import_facebook_succeed', ),
  ('metrics_import_facebook_fail', ),
  ('metrics_google_invite_attempt', ),
  ('metrics_google_invite_succeed', ),
  ('metrics_google_invite_fail', ),
  ('metrics_facebook_invite_attempt', ),
  ('metrics_facebook_invite_succeed', ),
  ('metrics_facebook_invite_fail', ),
  ('metrics_google_share_attempt', ),
  ('metrics_google_share_succeed', ),
  ('metrics_google_share_fail', ),
  ('metrics_drop_self', ),
  ('metrics_drop_favorite', ),
  ('metrics_drop_bar', ),
  ('metrics_drop_user', ),
  ('metrics_drop_nowhere', ),
  ('metrics_click_self', ),
  ('metrics_click_favorite', ),
  ('metrics_click_searchbar', ),
  ('metrics_transfer_self', ),
  ('metrics_transfer_favorite', ),
  ('metrics_transfer_user', ),
  ('metrics_transfer_social', ),
  ('metrics_transfer_email', ),
  ('metrics_panel_open', 'panel', ),
  ('metrics_panel_close', 'panel', ),
  ('metrics_panel_accept', 'panel', ),
  ('metrics_panel_deny', 'panel', ),
  ('metrics_panel_access', 'panel', ),
  ('metrics_panel_cancel', 'panel', 'author', ),
  ('metrics_dropzone_open', ),
  ('metrics_dropzone_close', ),
  ('metrics_dropzone_removeitem', ),
  ('metrics_dropzone_removeall', ),
  ('metrics_search', 'input', ),
  ('metrics_searchbar_search', ),
  ('metrics_searchbar_focus', ),
  ('metrics_searchbar_invite', 'input', ),
  ('metrics_searchbar_share', 'input', ),
  ('metrics_select_user', 'input', ),
  ('metrics_select_social', 'input', ),
  ('metrics_select_other', 'input', ),
  ('metrics_select_close', 'input', ),
]

generated_file = '// This file has been generated. Do not edit it.'

header_header = """%s
%s
"""

source_header = """%s

#include <surface/gap/metrics.hh>

using MKey = elle::metrics::Key;
%s
"""

prototype = """
gap_Status
%s%s(%s)"""

declaration = """%s
{
  assert(state != nullptr);

  WRAP_CPP_MANAGER(state,
                   reporter,
                   store,
                   "%s",
                   %s);
}"""

def gen_prototype(name, keys, prefix):
  def line(key):
    return "char const* %s" % key

  def keys_to_args(keys):
    out = ["gap_State* state"]
    out.extend([line(key) for key in keys])
    return ", ".join(out)

  return prototype % (prefix, name, keys_to_args(keys))

def gen_definition(name, keys = [], prefix = "gap_"):
  return gen_prototype(name, keys, prefix) + ";"

def gen_declaration(name, keys = [], prefix = "gap_"):
  def line(key, marker):
    return "{%s::%s, %s}" % (marker, key, key)

  def keys_to_args(keys):
    out = "{%s}" % ", ".join([line(key, "MKey") for key in keys])
    return out

  return declaration % (gen_prototype(name, keys, prefix), name, keys_to_args(keys))

import sys

def main(argv = None):
  if argv is None:
    argv = sys.argv

  dest_file = argv[1]
  with open("%s.h" % dest_file, "w") as dest_h, open("%s.hh" % dest_file, "w") as dest_c:

    dest_h.write(header_header % (generated_file,
                                  "\n".join([gen_definition(name, keys = args) for name, *args in prototypes])))
    dest_c.write(source_header % (generated_file,
                                  "\n".join([gen_declaration(name, keys = args) for name, *args in prototypes])))

if __name__ == "__main__":
  sys.exit(main())
