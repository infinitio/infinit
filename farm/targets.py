import os

arch, osyst, comp = os.environ['BUILDFARM_NAME'].split('-')

def targets(action):
  yield '//gap/%s' % action
  yield '//frete/%s' % action
  yield '//station/%s' % action
  yield '//papier/%s' % action

  # Only stock Ubuntus used in production and Mac Mini should run the server
  # tests.
  if osyst in ['linux_ubuntu_14', 'osx']:
    yield '//oracles/%s' % action
  else:
    yield '//oracles/meta/client/%s' % action
    yield '//oracles/trophonius/client/%s' % action
  # XXX: Until moc fix on Windows, only run fist-gui-qt/build target on linux.
  if osyst.startswith('linux_debian'):
    yield '//fist-gui-qt/%s' % action
  if action == 'build':
    yield '//python'
  # Run the elle and functional tests on the Mac Mini only.
  if action == 'check' and osyst == 'osx':
    yield '//elle/check'
    yield '//functional/check'
