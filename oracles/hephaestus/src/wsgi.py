import argparse
import infinit.oracles.hephaestus
import os
import sys

parser = argparse.ArgumentParser()
infinit.oracles.hephaestus.add_options(parser)
args = parser.parse_args()

application = infinit.oracles.hephaestus.Hephaestus(
  **infinit.oracles.hephaestus.get_options(args)
)
