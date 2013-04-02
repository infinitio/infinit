#!/usr/bin/env python2
# -*- encoding: utf-8 -*-

import os

import meta.conf

LISTEN_TCP_PORT = meta.conf.TROPHONIUS_PORT
LISTEN_SSL_PORT = meta.conf.TROPHONIUS_CONTROL_PORT

SSL_KEY = "pkey"
SSL_CERT = "cert"
