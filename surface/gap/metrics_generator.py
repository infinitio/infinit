#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

"""
Generate the C api for metrics.

"""

prototypes = [
# Connect.
  ('google_connect_attempt', ),
  ('google_connect_succeed', ),
  ('google_connect_fail', ),
  ('facebook_connect_attempt', ),
  ('facebook_connect_succeed', ),
  ('facebook_connect_fail', ),
# Login
  ('google_login_attempt', ),
  ('google_login_succeed', ),
  ('google_login_fail', ),
  ('facebook_login_attempt', ),
  ('facebook_login_succeed', ),
  ('facebook_login_fail', ),
# Import
  ('google_import_attempt', ),
  ('google_import_succeed', ),
  ('google_import_fail', ),
  ('facebook_import_attempt', ),
  ('facebook_import_succeed', ),
  ('facebook_import_fail', ),
# Invite
  ('google_invite_attempt', ),
  ('google_invite_succeed', ),
  ('google_invite_fail', ),
  ('facebook_invite_attempt', ),
  ('facebook_invite_succeed', ),
  ('facebook_invite_fail', ),
# Share
  ('google_share_attempt', ),
  ('google_share_succeed', ),
  ('google_share_fail', ),
# Drop.
  ('drop_self', ),
  ('drop_favorite', ),
  ('drop_bar', ),
  ('drop_user', ),
  ('drop_nowhere', ),
# Click.
  ('click_self', ),
  ('click_favorite', ),
  ('click_searchbar', ),
# Transfer.
  ('transfer_self', ),
  ('transfer_favorite', ),
  ('transfer_user', ),
  ('transfer_social', ),
  ('transfer_email', ),
  ('transfer_ghost', ),
# Panels.
  ('panel_open', 'panel', ),
  ('panel_close', 'panel', ),
  ('panel_accept', 'panel', ),
  ('panel_deny', 'panel', ),
  ('panel_access', 'panel', ),
  ('panel_cancel', 'panel', 'author', ),
# Dropzone.
  ('dropzone_open', ),
  ('dropzone_close', ),
  ('dropzone_removeitem', ),
  ('dropzone_removeall', ),
# Search.
  ('search', 'input', ),
  ('searchbar_search', ),
  ('searchbar_focus', ),
  ('searchbar_invite', 'input', ),
  ('searchbar_share', 'input', ),
# Selection.
  ('select_user', 'input', ),
  ('select_social', 'input', ),
  ('select_ghost', 'input', ),
  ('select_favorite', 'input', ),
  ('select_close', 'input', ),
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

def gen_definition(name, keys = [], prefix = "gap_metrics_"):
  return gen_prototype(name, keys, prefix) + ";"

def gen_declaration(name, keys = [], prefix = "gap_metrics_"):
  def line(key, marker):
    return "{%s::%s, %s}" % (marker, key, key)

  def keys_to_args(keys):
    out = "{%s}" % ", ".join([line(key, "MKey") for key in keys])
    return out

  return declaration % (gen_prototype(name, keys, prefix),
                        name,
                        keys_to_args(keys))

import sys

def main(argv = None):
  if argv is None:
    argv = sys.argv

  dest = argv[1]
  with open("%s.h" % dest, "w") as dest_h, open("%s.hh" % dest, "w") as dest_c:

    dest_h.write(header_header % (generated_file,
                                  "\n".join([gen_definition(name, keys = keys)
                                             for name, *keys in prototypes])))
    dest_c.write(source_header % (generated_file,
                                  "\n".join([gen_declaration(name, keys = keys)
                                             for name, *keys in prototypes])))

if __name__ == "__main__":
  sys.exit(main())
