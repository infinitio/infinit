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
    def __init__(self, ip="127.0.0.1", port=conf.LISTEN_TCP_PORT, ssl_port=conf.LISTEN_SSL_PORT, logfile=sys.stderr):
        self.ip = ip
        self.port = port
        self.logfile = logfile
        self.ssl_port = ssl_port
        self.clients = dict()
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


    def reset_user_status(self):
        """XXX We reset all user's status to be "disconnected". We know it's
        true because there is only on trophonius instance at the moment.
        The right way to do it is to disconnect all users (in the database)
        when the trophonius instance goes down.
        """
        import meta
        meta.database().users().update({}, {'$set': {'connected': False}})

    def run(self):
        self.reset_user_status()

        if not all(os.path.exists(file) for file in (conf.SSL_KEY, conf.SSL_CERT)):
            self.create_self_signed_cert(".")
        log.startLogging(self.logfile)

        factory = trophonius.TrophoFactory(self)
        meta_factory = trophonius.MetaTrophoFactory(self)
        reactor.listenTCP(self.port, factory)
        reactor.listenTCP(self.ssl_port, meta_factory)
        reactor.run()
