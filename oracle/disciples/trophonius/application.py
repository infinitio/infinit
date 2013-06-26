#!/usr/bin/env python2
# -*- encoding: utf-8 -*-

from __future__ import print_function

import sys
import conf

from twisted.internet import reactor

from twisted.internet import reactor, protocol
from twisted.python import filepath
from twisted.python import log

from OpenSSL import crypto

import trophonius
import clients
import os
import os.path
import pythia.constants

try:
    import setproctitle
    HAVE_SETPROCTITLE = True
except:
    HAVE_SETPROCTITLE = False
    pass

class Message(object):
    def __init__(self, type, **kwars):
        self.type = type

class Application(object):
    def __init__(self, ip="127.0.0.1",
            port=conf.LISTEN_TCP_PORT,
            ssl_port=conf.LISTEN_SSL_PORT,
            logfile=sys.stderr,
            meta_url=pythia.constants.DEFAULT_SERVER,
            runtime_dir=None):
        self.ip = ip
        self.port = port
        self.logfile = logfile
        self.ssl_port = ssl_port
        self.clients = dict()
        self.meta_url = meta_url
        self.runtime_dir = runtime_dir
        if HAVE_SETPROCTITLE:
            setproctitle.setproctitle("Trophonius")

    def create_self_signed_cert(self, cert_dir):
        """
        If datacard.crt and datacard.key don't exist in cert_dir, create a new
        self-signed cert and keypair and write them into that directory.
        """
        # create a key pair
        k = crypto.PKey()
        k.generate_key(crypto.TYPE_RSA, 2048)

        # create a self-signed cert
        cert = crypto.X509()
        cert.get_subject().C = "FR"
        cert.get_subject().ST = "France"
        cert.get_subject().L = "Paris"
        cert.get_subject().O = "Infinit"
        cert.get_subject().OU = "Infinit.io"
        cert.get_subject().CN = "Ta mere"
        cert.set_serial_number(1000)
        cert.gmtime_adj_notBefore(0)
        cert.gmtime_adj_notAfter(10*365*24*60*60)
        cert.set_issuer(cert.get_subject())
        cert.set_pubkey(k)
        cert.sign(k, 'sha256')

        self.ssl_key_path = os.path.join(cert_dir, conf.SSL_KEY)
        self.ssl_cert_path = os.path.join(cert_dir, conf.SSL_CERT)
        open(self.ssl_cert_path, "wt").write(
                crypto.dump_certificate(crypto.FILETYPE_PEM, cert))
        open(self.ssl_key_path, "wt").write(
                crypto.dump_privatekey(crypto.FILETYPE_PEM, k))

    def make_portfiles(self):
        with open(os.path.join(self.runtime_dir, "trophonius.sock", ), 'w+') as f:
            port = self.port.getHost().port
            f.write(str(port))
        with open(os.path.join(self.runtime_dir, "trophonius.csock", ), 'w+') as f:
            port = self.control_port.getHost().port
            f.write(str(port))

    def run(self):
        if not all(os.path.exists(file) for file in (conf.SSL_KEY, conf.SSL_CERT)):
            self.create_self_signed_cert(".")
        log.startLogging(self.logfile)

        factory = trophonius.TrophoFactory(self)
        meta_factory = trophonius.MetaTrophoFactory(self)
        self.port = reactor.listenTCP(self.port, factory)
        self.control_port = reactor.listenTCP(self.ssl_port, meta_factory)
        if self.runtime_dir is not None:
            self.make_portfiles()
        reactor.run()
