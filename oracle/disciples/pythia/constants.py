# -*- encoding: utf-8 -*-
import os

ADMIN_TOKEN = "fdjskfdakl;asdklwqioefwiopfdsjkl;daskl;askl;fsd"
_META_PORT = 'INFINIT_META_PORT' in os.environ and os.environ['INFINIT_META_PORT'] or 12345
DEFAULT_SERVER = 'http://localhost:%i' % int(_META_PORT)
RESET_PASSWORD_VALIDITY = 2 * 3600 # 2 hours
