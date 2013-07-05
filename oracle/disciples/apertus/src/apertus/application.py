from __future__ import print_function

import sys


from OpenSSL import SSL, crypto

from twisted.internet.protocol import Protocol
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
import netifaces

import meta.conf

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
    def __init__(self,
                 ip = meta.conf.APERTUS_HOST,
                 port = meta.conf.APERTUS_PORT,
                 runtime_dir = ""):
        self.ip = ip
        self.port = port
        self.runtime_dir = runtime_dir

    def local_ip(self):
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("gmail.com", 80))
        ip, port = s.getsockname()
        s.close()
        return ip

    def run(self):
        log.startLogging(sys.stderr)

        addr = self.local_ip()
        factory = apertus.Factory(addr)

        listening_port = reactor.listenTCP(self.port, factory)

        if self.runtime_dir:
            port = listening_port.getHost().port
            with open(os.path.join(self.runtime_dir, "apertus.sock"), "w+") as portfile:
                portfile.write("control:{}\n".format(port))

        if HAVE_SETPROCTITLE:
            setproctitle("apertus-server")
        reactor.run()
