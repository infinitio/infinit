import meta
import trophonius

class Servers:

    def __init__(self):
        self.meta = None
        self.tropho = None

    def __enter__(self):
        port = 39075 # XXX
        self.meta = meta.Meta(
            spawn_db = True,
            trophonius_control_port = port)
        self.meta.__enter__()
        self.tropho = trophonius.Trophonius(
            meta_port = self.meta.meta_port,
            control_port = port)
        self.tropho.__enter__()
        return self.meta, self.tropho

    def __exit__(self, exception_type, exception, *args):
        self.tropho.__exit__(exception_type, exception, *args)
        self.meta.__exit__(exception_type, exception, *args)
        if exception is not None and exception_type is not AssertionError:
            print('======== Trophonius stdout:\n' + self.tropho.stdout,
                  file = sys.stderr)
            print('======== Trophonius stderr:\n' + self.tropho.stderr,
                  file = sys.stderr)
            print('======== Meta stdout:\n' + self.meta.stdout,
                  file = sys.stderr)
            print('======== Meta stderr:\n' + self.meta.stderr,
                  file = sys.stderr)
