#!/usr/bin/env python3

import traceback

import os, sys
sys.path.insert(
    0,
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), '../../elle/drake/src')
    )
)

import drake
import drake.cxx
import drake.cxx.boost
import drake.cxx.qt

cxx_toolkit = drake.cxx.GccToolkit(
  compiler = os.getenv('CXX', 'x86_64-w64-mingw32-g++'))
cxx_config = drake.cxx.Config()
cxx_config.flag('-fPIC')
cxx_config.flag('-Wall')
cxx_config.enable_debug_symbols()
cxx_config.enable_optimization(False)

boost = drake.cxx.boost.Boost(
  prefix = os.getenv('BOOST_PREFIX', '/usr'),
  cxx_toolkit = cxx_toolkit,
  prefer_shared = False)

try:
  qt = drake.cxx.qt.Qt(
    prefix = os.getenv('QT_PREFIX', '/usr'),
    gui = True,
  )
except drake.Exception:
  print("Qt not found, all targets releated will be ignored")
  qt = None

python3 = drake.cxx.find_library(
  token = 'pyconfig.h',
  prefix = os.getenv('PYTHON_PREFIX', '/usr'),
  libs = ('python3',),
  toolkit = cxx_toolkit)

drake.run('../..',
          build_type = 'Development',
          cxx_toolkit = cxx_toolkit,
          cxx_config = cxx_config,
          python3 = python3,
          boost = boost,
          qt = qt,
          enable_horizon = False)
