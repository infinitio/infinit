import string
import random

def generator(size = 12, prefix = "", chars = string.ascii_lowercase):
    return prefix + ''.join(random.choice(chars) for x in range(size))

def generate_identity(*a, **ka):
  """Return an identity and a public key.
  id -- the id of the user.
  email -- the email of the user.
  password -- the client side hashed password.
  authority_path -- the path of the server authority.
  authority_password -- the password of the server authority.
  """
  return generator(size = 36), generator(size = 36)

def generate_passport(*a, **ka):
  """Return a passport for a given user.
  id -- the device id.
  name -- the device name.
  public_key -- the owner public key.
  authority_path -- the path of the server authority.
  authority_password -- the password of the server authority.
  """
  return generator(size = 36)
