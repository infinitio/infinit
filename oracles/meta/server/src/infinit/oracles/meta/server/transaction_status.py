import os.path
import sys
import re

from infinit.oracles.transaction import statuses

for name, value in statuses.items():
  globals()[name.upper()] = value

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
      FAILED,
      FINISHED,
      GHOST_UPLOADED,
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
  GHOST_UPLOADED: {True: [CANCELED], False: [ACCEPTED, REJECTED, CANCELED, FAILED, FINISHED]},
  FINISHED: {True: [FINISHED, CANCELED, FAILED], False: [FINISHED, CANCELED, FAILED]},
  CANCELED: {True: [CANCELED, FAILED], False: [CANCELED, FAILED]},
  FAILED: {True: [CANCELED, FAILED], False: [CANCELED, FAILED]},
  REJECTED: {True: [CANCELED, FAILED], False: [REJECTED, CANCELED, FAILED]}
  }
