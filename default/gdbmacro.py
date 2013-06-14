# -*- coding: utf-8 -*-
"""
test script for gdb
"""
from __future__ import print_function
from pprint import pprint
import gdb

gdb_script = """\
echo \\nsource info:\\n
info source
echo \\ndisplaying lines around the fault:\\n
list
echo \\ndisplay info on threads:\\n
info threads
echo \\ndisplaying all threads bt:
thread apply all bt
echo \\ndisplaying all threads FULL bt (can be very verbose):
thread apply all bt full
quit
"""


def show_sources(file, line):
    line = line - 1
    try:
        with open(file, "r") as source:
            for text_line, text in enumerate(source):
                if (text_line > line - 5 and text_line < line + 5) and text_line != line:
                    print(">  ", text, end="")
                if text_line == line:
                    print("\033[32m-->", text, end="\033[0m")
    except IOError as e:
        print(e)

class BlockBt(gdb.Command):
    """
    Show the backtrace with the blocks of code
    """
    
    def __init__(self):
        super(BlockBt, self).__init__("block_bt", gdb.COMMAND_SUPPORT)

    def invoke(self, args, tty):
        frame =  gdb.selected_frame()
        while 1:
            frame = frame.older()
            if frame is None:
                break
            if frame.function():
                print(frame.function().print_name)
            else:
                print("???")
                continue
            block = frame.block()
            print("locals vars: ")
            has_some = False
            for b in block:
                has_some = True
                print(" - {} +{}: {} {} = {}".format(
                    b.symtab.fullname(),
                    b.line,
                    b.type,
                    b.print_name,
                    frame.read_var(b)))
                print("declared here:")
                show_sources(b.symtab.fullname(), b.line)
                print("")
            if not has_some:
                print("block has no locals")
BlockBt()
for command in gdb_script.split("\n"):
    gdb.execute(command, from_tty = False)
