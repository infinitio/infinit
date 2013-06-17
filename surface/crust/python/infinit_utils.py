def format_string(string):
    if not isinstance(string, str) or len(string) == 0:
        return string
    if string[-1:] != ".":
        string = string + "."
    return string[0].upper() + string[1:]

def run(parser, main):
    import sys
    try:
        args = parser.parse_args()
        main(args)
        sys.exit(0)
    except Exception as e:
        print(format_string(str(e)))
        sys.exit(1)
