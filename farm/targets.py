import os

arch, osyst, comp = os.environ['BUILDFARM_NAME'].split('-')

def targets(action):
  yield '//gap/%s' % action
  yield '//frete/check' % action
  yield '//station/check' % action
  if osyst.startswith('linux'):
    yield '//oracles/%s' % action
  else:
    yield '//oracles/meta/client/%s' % action
    yield '//oracles/trophonius/client/%s' % action
