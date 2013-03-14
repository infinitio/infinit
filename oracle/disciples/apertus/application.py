from __future__ import print_function

import sys

if "bsd" in sys.platform:
    from twisted.internet import kqreactor as _platform_reactor
elif "linux" in sys.platform:
    from twisted.internet import epollreactor as _platform_reactor
elif sys.platform.startswith('win'):
    from twisted.internet import iocpreactor as _platform_reactor
else:
    from twisted.internet import selectreactor as _platform_reactor

_platform_reactor.install()

from OpenSSL import SSL, crypto

from twisted.internet.protocol import Factory, Protocol
from twisted.internet import reactor, ssl
from twisted.python import log

try:
    from setproctitle import setproctitle
    HAVE_SETPROCTITLE = True
except:
    HAVE_SETPROCTITLE = False
    pass

import os
import conf
import apertus

def create_self_signed_cert(cert_dir = "."):
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

    ssl_key_path = os.path.join(cert_dir, conf.SSL_KEY)
    ssl_cert_path = os.path.join(cert_dir, conf.SSL_CERT)
    open(ssl_cert_path, "wt").write(
            crypto.dump_certificate(crypto.FILETYPE_PEM, cert))
    open(ssl_key_path, "wt").write(
            crypto.dump_privatekey(crypto.FILETYPE_PEM, k))
    return ssl_cert_path, ssl_key_path

def verify_cb(connection, x509, errnum, errdepth, ok):
    if not ok:
        print("Invalid cert from user", x509.get_subjet())
        return False
    else:
        print("Ok !")
    return True

def start_ssl(port, factory):
        cert, key = create_self_signed_cert()
        ssl_factory = ssl.DefaultOpenSSLContextFactory(key, cert)
        ssl_ctx = ssl_factory.getContext()
        ssl_ctx.set_verify(
                SSL.VERIFY_PEER | SSL.VERIFY_FAIL_IF_NO_PEER_CERT,
                verify_cb
        )
        reactor.listenSSL(port, factory, ssl_factory)

class Application(object):
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        pass

    def run(self):
        factory = Factory()
        factory.protocol = apertus.ApertusControl
        factory.slave = apertus.Apertus()
        log.startLogging(sys.stderr)
        reactor.listenUDP(self.port, factory.slave)
        reactor.listenTCP(self.port + 1, factory, interface="localhost")
        if HAVE_SETPROCTITLE:
            setproctitle("apertus-server")
        reactor.run()
