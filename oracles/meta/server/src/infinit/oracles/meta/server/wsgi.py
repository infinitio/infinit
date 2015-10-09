import infinit.oracles.meta.server
import infinit.oracles.emailer
import infinit.oracles.smser
import os

os.environ['META_LOG_SYSTEM'] = 'meta'
os.environ['ELLE_LOG_LEVEL'] = 'infinit.oracles.meta.*:DEBUG'

swu_key = 'live_7e775f6f0e1404802a5fbbc0fcfa9c238b065c49'
emailer = infinit.oracles.emailer.SendWithUsEmailer(api_key = swu_key)
stripe_key = 'sk_live_gplnR4VlHoO843kfacKkk591'

from infinit.oracles.meta.server.gcs import GCS
gcs_login = '798530033299-s9b7qmrc99trk8uid53giuvus1o74cif@developer.gserviceaccount.com'
gcs_key = bytes('''-----BEGIN PRIVATE KEY-----
MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBALCm3D3cHlKYRygk
vRgesY39WUGeUN/sCBsVaxMuga1bCAZ6fVoh58pQEmeBpkjaVdtB0nz9ZBVoeDtR
PcfafaUW+UFXjRf2rJ3MoJ/J72mccSD08sjVX3Q9U5iydYhjZEx3uwhUcaHG6+Rq
f4xhb/49jfFmDJ/9zCopsiPBJQgfAgMBAAECgYEAqxgByrxOdirdCGmE6D6aM+8E
qwReSnL+atT0zzBFExVPEY9Dp6+dI5soKC4vUvJ9I45+AucdL4ruoG0QTGg3NbjC
XCD88TL2UdSog/xxHAQ37EvnoPwK6v04FZHdm94eXkJMQzpf9pP8EyVEaXZWb8Uw
2MDPGluTWgkUKZitkLECQQDjuLBFwtU9rdDZB3G00P3hMXuvomPPEHRvdpvwbxLG
WX1XNPG1FlBbQhyBgUIVATn9sU28df7kANqhhnEthXY3AkEAxpaoR0rtZzPIt4c4
3PQm+mclxxEUZozrRnO/t6bDc/wGvI7C69wIu4UI8j4zFtRRuC2qCDaTorXibFRb
PKEJWQJAY8eNFUQlg30hwbbNT9kzJPU1qOOSsCwZmK1z7on8xAR6MzfzoNFCLHpv
Wx90ARgkfNCvqyBYqzbklVn/RV7xSQJBAJluCPGb+DPGFIuHU+2STRMl4lAc6BAb
TCOQhk0T8OqJi4LfIcYsqCqJLFJMsBgxTjnoPfg+gm4x7JAZ1KvRF3ECQFcwSrNV
cun1SplfUKZQZywA8ueUU/ZuGj/XXwopPR5LgWW7sgkwdCklQUPjcecWEZFy/ODl
e9FGZj7sEHpPuDE=
-----END PRIVATE KEY-----
''', 'UTF-8')
gcs = GCS(login = gcs_login, key = gcs_key)

smser = infinit.oracles.smser.NexmoSMSer(nexmo_api_secret = 'ac557312')

application = infinit.oracles.meta.server.Meta(
  mongo_replica_set = ['mongo-0', 'mongo-1', 'mongo-2',],
  aws_region = 'us-east-1',
  aws_buffer_bucket = 'us-east-1-buffer-infinit-io',
  aws_invite_bucket = 'us-east-1-invite-infinit-io',
  aws_link_bucket = 'us-east-1-links-infinit-io',
  emailer = emailer,
  smser = smser,
  stripe_api_key = stripe_key,
  gcs = gcs,
  production = True,
)
