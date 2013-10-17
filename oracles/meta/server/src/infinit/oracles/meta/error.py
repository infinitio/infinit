import re
import os.path
import sys

_macro_matcher = re.compile(r'(.*\()(\S+)(,.*\))')

def replacer(match):
    field = match.group(2)
    return match.group(1) + "'" + field + "'" + match.group(3)

def ERR_CODE(name, value, comment):
    globals()[name.upper()] = (value, comment)

filepath = os.path.abspath(
    os.path.join(os.path.dirname(__file__), 'error_code.hh.inc')
)

configfile = open(filepath, 'r')
for line in configfile:
    eval(_macro_matcher.sub(replacer, line))

class Error(Exception):

  def __init__(self, error_code, error_message = ""):
    super().__init__(error_code,
                     len(error_message) == 0 and error_code[1] or error_message)
