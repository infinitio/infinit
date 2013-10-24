import os.path
import sys
import re

_macro_matcher = re.compile(r'(.*\()(\S+)(,.*\))')

def replacer(match):
    field = match.group(2)
    return match.group(1) + "'" + field + "'" + match.group(3)

_status_to_string = dict();

def TRANSACTION_STATUS(name, value):
    globals()[name.upper()] = value
    _status_to_string[value] = str(name)

filepath = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'transaction_status.hh.inc')
)

configfile = open(filepath, 'r')
for line in configfile:
    eval(_macro_matcher.sub(replacer, line))

final = [REJECTED, FAILED, FINISHED, CANCELED]

transitions = {
  CREATED:
    {
    True: [
      INITIALIZED,
      CANCELED,
      FAILED
      ],
    False: [],
    },
  INITIALIZED:
    {
    True: [
      CANCELED,
      FAILED
      ],
    False: [
      ACCEPTED,
      REJECTED,
      CANCELED,
      FAILED
      ]
    },
  ACCEPTED:
    {
    True: [
      CANCELED,
      FAILED
      ],
    False: [
      ACCEPTED,
      FINISHED,
      CANCELED,
      FAILED
      ]
    },
  FINISHED: {True: [CANCELED, FAILED], False: [FINISHED, CANCELED, FAILED]},
  CANCELED: {True: [CANCELED, FAILED], False: [CANCELED, FAILED]},
  FAILED: {True: [CANCELED, FAILED], False: [CANCELED, FAILED]},
  REJECTED: {True: [CANCELED, FAILED], False: [REJECTED, CANCELED, FAILED]}
  }
