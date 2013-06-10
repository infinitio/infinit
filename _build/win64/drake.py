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

#cxx_toolkit = drake.cxx.GccToolkit()
#cxx_config = drake.cxx.Config()
#cxx_config.flag('-fPIC')
#cxx_config.flag('-Wall')
#cxx_config.enable_debug_symbols()
#cxx_config.enable_optimization(False)

cxx_toolkit = drake.cxx.VisualToolkit(
  override_path = 'C:\\Program Files (x86)\\Microsoft Visual C++ Compiler Nov 2012 CTP\\bin',
  override_include = 'C:\\Program Files (x86)\\Microsoft Visual C++ Compiler Nov 2012 CTP\\include')
cxx_config = drake.cxx.Config()
cxx_config.define('WIN32_LEAN_AND_MEAN')

drake.run('../..',
          build_type = 'Development',
          cxx_toolkit = cxx_toolkit,
          cxx_config = cxx_config,
          enable_horizon = False)
