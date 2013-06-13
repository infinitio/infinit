# -*- encoding: utf-8 -*-

"""
pages module provides all page class
"""

from meta.resources import root
from meta.resources import user
from meta.resources import device
from meta.resources import network
from meta.resources import transaction
from meta.resources import notification
from meta.resources import authority
from meta.resources import descriptor

from meta.page import Page

_modules = [root, user, device, network, transaction, notification, authority, descriptor]

ALL = []

for module in _modules:
    for obj in module.__dict__.values():
        try:
            if issubclass(obj, Page) and obj != Page and obj.__name__[0] != '_':
                ALL.append(obj)
        except:
            pass
