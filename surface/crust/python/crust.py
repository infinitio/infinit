from _crust import Network

def create(name, identity, model, openess, policy, store=None, publish=tuple()):
  from getpass import getpass
  password = getpass("password: ")
  net = Network(name, identity, password, model, openess, policy)
  if store:
    net.store(store)

if __name__ == "__main__":
  import argparse

  actions=["create", "delete", "user"]

  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest="action")

  # Create
  parser_create = subparsers.add_parser("create")
  parser_create.add_argument("name", help="The name of the network")
  parser_create.add_argument("identity", help="The path to your user identity")
  parser_create.add_argument("-m", "--model",
                             choices=['slug', 'local', 'remote', 'cirkle', 'kool'],
                             default='slug',
                             help="The model of the network.")
  parser_create.add_argument("-o", "--openness",
                             choices=['open', 'community', 'closed'],
                             default='open',
                             help="The openess of the network.")
  parser_create.add_argument("-p", "--policy",
                             choices=['accessible', 'editable', 'confidential'],
                             default='accessible',
                             help="The policy of the network.")
  parser_create.add_argument("-s", "--store",
                             help="The place to store")
  parser_create.add_argument("-t", "--publish",
                             help="bite")

  # Delete
  parser_delete = subparsers.add_parser("delete")
  parser_delete.add_argument("network", help="The descriptor file of the network")

  # AddUser
  parser_user = subparsers.add_parser("user")
  parser_user_sub = parser_user.add_subparsers(dest="subaction")

  parser_user_add = parser_user_sub.add_parser("add")
  parser_user_add.add_argument("network", help="The descriptor file of the network.")
  parser_user_add.add_argument("key", help="The public key of the user to add.")
  parser_user_remove = parser_user_sub.add_parser("remove")
  parser_user_remove.add_argument("network", help="The descriptor file of the network.")
  parser_user_remove.add_argument("key", help="The public key of the user to remove.")
  parser_user_list = parser_user_sub.add_parser("list")
  parser_user_list.add_argument("network", help="The descriptor file of the network.")

  # args = parser.parse_args()
  args = parser.parse_args()

  print(args)
  if args.action == 'create':
    create(args.name,
           args.identity,
           args.model,
           args.openness,
           args.policy)
  elif args.action == 'delete':
    pass
  elif args.action == 'user':
    if args.subaction == 'add':
      pass
    elif args.subaction == 'delete':
      pass
    elif args.subaction == 'list':
      pass
  else:
    raise "Invalid arg"

  #import _crust
