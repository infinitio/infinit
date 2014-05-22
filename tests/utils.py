import os
import shutil
import filecmp
import tempfile

def run(f):
  import reactor
  s = reactor.Scheduler()
  t = reactor.Thread(s, 'main', f)
  s.run()


def expect_raise(f, type = BaseException):
  try:
    f()
  except type:
    pass
  else:
    raise Exception('expected a %s' % type)


class TemporaryData:
  """ Create temporary file(s) with given names and sizes

  @param name,names: File names (may contain directory elements)

  Will make the following properties available:
  full_path : full path of first file
  full_pathes: full path of all files
  directory: Directory containing generated files (and nothing else)
  """
  def __init__(self, name=None, size=None, names = []):
    self.size = size
    self.names = names
    if name is not None:
      self.names = [name] + self.names
    self.full_path = None
    self.full_pathes = []
    if size is None:
      raise Exception('Size must be set')
  def __enter__(self):
    self.directory = tempfile.mkdtemp('infinit-test-files')
    for n in self.names:
      path = os.path.join(self.directory, n)
      d = os.path.dirname(path)
      # Makedirs fail if last component exists with incorrect mode
      if len(os.path.dirname(n)):
        os.makedirs(d,  exist_ok = True)
      self.full_pathes.append(path)
      with open(path, 'wb') as f:
        f.write(bytes('a'* self.size,'ascii'))
    self.full_path = self.full_pathes[0]
    return self
  def __exit__(self, *args, **kvargs):
    shutil.rmtree(self.directory)

# Callable that compares two files or directories for identical content.
class FileMatch:
  def __init__(self, f1, f2):
    self.f1 = f1
    self.f2 = f2
  def explain(self):
    for p in [self.f1, self.f2]:
      print('------- %s (%s)' % (p, os.path.exists(p)))
      for (dirpath, dirnames, filenames) in os.walk(p):
        for f in filenames:
          full = os.path.join(dirpath, f)
          print('%s: %s' % (full, os.path.getsize(full)))
    print('--------')
  def __call__(self):
    try:
      if os.path.isdir(self.f1):
        # Join the relative content file names in both dirs
        content = set()
        for p in [self.f1, self.f2]:
          for (dirpath, dirnames, filenames) in os.walk(p):
            rel = os.path.relpath(dirpath, p)
            for f in filenames:
              content.add(os.path.join(rel, f))
        (ok, fail, err) = filecmp.cmpfiles(self.f1, self.f2, list(content), shallow = False)
        return len(fail) == 0 and len(err) == 0
      else:
        return filecmp.cmp(self.f1, self.f2, list(content), shallow = False)
    except Exception as e:
      return False # Probably one of them do not exist
