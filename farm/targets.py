import os

arch, osyst, comp = os.environ['BUILDFARM_NAME'].split('-')

def targets(action):
  yield '//gap/%s' % action
  yield '//frete/%s' % action
  yield '//station/%s' % action
  yield '//papier/%s' % action
  if osyst.startswith('linux'):
    yield '//oracles/%s' % action
  else:
    yield '//oracles/meta/client/%s' % action
    yield '//oracles/trophonius/client/%s' % action
  if arch == 'i686' and osyst == 'win':
    yield '//fist-gui-qt/build'
