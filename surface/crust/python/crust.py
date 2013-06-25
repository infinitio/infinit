#!/usr/bin/env python3
# -*- encoding: utf-8 -*-

from pycrust import Network

def create(name, identity, model, openess, policy, store-):
  from getpass import getpass
  password = getpass("password: ")
  net = Network(name, identity, password, model, openess, policy)
  if store:
    net.store(store)
  if publish:
    net.publish(host, int(port), tokenpath)
  if install:
    net.install(install)

if __name__ == "__main__":
  import argparse

  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest="action")

  # Create
  parser_create = subparsers.add_parser("create")
  parser_create.add_argument("name", help = "The name of the network")
  parser_create.add_argument("identity", help = "The path to your user identity")
  parser_create.add_argument("-m", "--model",
                             choices = ['slug', 'local', 'remote', 'cirkle', 'kool'],
                             default = 'slug',
                             help = "The model of the network.")
  parser_create.add_argument("-o", "--openness",
                             choices=['open', 'community', 'closed'],
                             default='open',
                             help = "The openess of the network.")
  parser_create.add_argument("-p", "--policy",
                             choices=['accessible', 'editable', 'confidential'],
                             default='accessible',
                             help = "The policy of the network.")
  parser_create.add_argument("-s", "--store",
                             help = "The place to store")
  parser_create.add_argument("-t", "--publish",
                             help = "Save the network on the server.")
  parser_create.add_argument("-i", "--install",
                             help = "Create the directory.")
  parser_create.add_argument("--mount",
                             help = "Create the directory.")
  parser_create.add_argument("--host",
                             help = "The meta host. You can also use INFINIT_META_HOST.")
  parser_create.add_argument("--port",
                             help = "The meta port. You can also use INFINIT_META_PORT.")
  parser_create.add_argument("--tokenpath",
                             help = "The path to the master token. You can also use INFINIT_META_TOKEN.")
  # Publish


  # Delete
  parser_delete = subparsers.add_parser("delete")
  parser_delete.add_argument("network", help = "The descriptor file of the network")

  # # AddUser
  # parser_user = subparsers.add_parser("user")
  # parser_user_sub = parser_user.add_subparsers(dest="subaction")

  # parser_user_add = parser_user_sub.add_parser("add")
  # parser_user_add.add_argument("network", help = "The descriptor file of the network.")
  # parser_user_add.add_argument("key", help = "The public key of the user to add.")
  # parser_user_remove = parser_user_sub.add_parser("remove")
  # parser_user_remove.add_argument("network", help = "The descriptor file of the network.")
  # parser_user_remove.add_argument("key", help = "The public key of the user to remove.")
  # parser_user_list = parser_user_sub.add_parser("list")
  # parser_user_list.add_argument("network", help = "The descriptor file of the network.")

  # args = parser.parse_args()
  args = parser.parse_args()

  print(args)
  if args.action == 'create':
    create(name = args.name,
           identity = args.identity,
           model = args.model,
           openess = args.openness,
           policy = args.policy,
           store = args.store,
           publish = args.publish,
           install = args.install,
           mount = args.mount,
           host = args.host,
           port = args.port,
           tokenpath = args.tokenpath,
           )
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
