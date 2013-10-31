import os.path
import sys
import re

statuses = {
  'accepted': 2,
  'canceled': 6,
  'created': 0,
  'failed': 7,
  'finished': 4,
  'initialized': 1,
  'none': 998,
  'rejected': 5,
  'started': 999,
}
