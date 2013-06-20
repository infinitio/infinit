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
                 control_port = meta.conf.APERTUS_CONTROL_PORT):
        self.ip = ip
        self.port = port
        self.control_port = control_port
        pass

    def run(self):
        log.startLogging(sys.stderr)

        interface_names = ['eth0', 'en0', 'en1', 'eno1', 'eno2']
        found = False
        for name in interface_names:
            try:
                iface = netifaces.ifaddresses(name)
                l_addr4 = iface[netifaces.AF_INET]
                l_addr6 = iface[netifaces.AF_INET6]
            except:
                pass
            else:
                found = True
                break
        if not found:
            raise Exception("Cannot find valid interface")

        factory = apertus.Factory(l_addr4[0]['addr'])

        reactor.listenTCP(self.control_port, factory, interface="localhost")

        if HAVE_SETPROCTITLE:
            setproctitle("apertus-server")
        reactor.run()
