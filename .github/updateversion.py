import re
import sys

if len(sys.argv) < 2:
    print("Usage: python3 updateversion.py <version>")
    sys.exit(1)

tag = sys.argv[1]

content_new = ''
with open ('./platformio.ini', 'r' ) as f:
    content = f.read()
    content_new = re.sub('pixelit_version\s*=\s*.*', 'pixelit_version = ' + tag, content, flags = re.M)
    f.close()

if content_new != "":
    with open ('./platformio.ini', 'w' ) as f:
        f.write(content_new)
        f.close()
