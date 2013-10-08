import bottle

def load_shared_header(path, matcher, repacer, func):
  file = open(filepath, 'r')
  for line in file:
    eval(matcher.sub(replacer, line))

class expect_json:
  def __init__(self, expect_keys = [], explode_keys = []):
    assert isinstance(expect_keys, (list, set))
    assert isinstance(explode_keys, (list, set))
    self.mandatory_fields = set(expect_keys + explode_keys)
    self.explode_keys = explode_keys
  def __call__(self, method, *args, **kwargs):
    def decorator_wrapper(*args, **kwargs):
      try:
        input = bottle.request.json
        if input is None:
          bad_request("body is not JSON")
      except ValueError:
        bad_request("body can't be decoded as JSON")
      json = dict(input)
      for key in self.mandatory_fields:
        if key not in json.keys():
          bad_request("missing key: %r" % key)
      # Probably optimisable.
      exploded = {key: json[key] for key in self.explode_keys}
      optionals = {key: json[key] for key in json.keys() - self.explode_keys}
      return method(*args, optionals = optionals, **exploded)
    return decorator_wrapper

# There is probably a better way.
def stringify_object_ids(obj):
  if isinstance(obj, ObjectId):
    return str(obj)
  if hasattr(obj, '__iter__'):
    if isinstance(obj, list):
      return [stringify_object_ids(sub) for sub in obj]
    elif isinstance(obj, pymongo.cursor.Cursor):
      return [stringify_object_ids(sub) for sub in obj]
    elif isinstance(obj, dict):
      return {key: stringify_object_ids(obj[key]) for key in obj.keys()}
    elif isinstance(obj, set):
      return (stringify_object_ids(sub) for sub in obj)
    elif isinstance(obj, str):
      return obj
    else:
      raise TypeError("unsported type %s" % type(obj))
  return obj
