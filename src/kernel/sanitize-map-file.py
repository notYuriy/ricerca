# Script that sanitizes map file and generates kernel module for stacktraces
# Original map file gets overwritten
import sys
import struct

with open(sys.argv[1], 'r') as mapfile:
    # To simplify parsing, lines are split beforehand
    lines = list(map(lambda line: line.split(), mapfile.readlines()))

# Collect all lines that are in .text section and consist of two components
# TODO: come up with a better way to search for function symbols
textlines = []
intext = False
for line in lines:
    if len(line) == 0:
        continue
    if line[0].startswith('.'):
        intext = line[0].startswith('.text')
        continue
    if intext and len(line) == 2:
        textlines.append(line)

# Convert lines to addr,name tuples and sort by address
functions = sorted(list(map(lambda line: (int(line[0], 16), line[1]), textlines)))
print(functions)

# Overwrite map file with a better version
with open(sys.argv[1], 'wb') as mapfile:
    # Signature (0x10203040) and functions count
    mapfile.write(struct.pack('QQ', 0x1020304050607080, len(functions)))
    # Function addresses
    for function in functions:
        mapfile.write(struct.pack('Q', function[0]))
    # Strings offset
    # 16 for signature and functions count
    # 8 * len(functions) for address data
    # 4 * len(functions) for relative pointers to symbol names
    nameTableOffset = 16 + 12 * len(functions)
    # Emit string table offsets
    currentStringOffset = nameTableOffset
    for function in functions:
        name = function[1]
        mapfile.write(struct.pack('I', currentStringOffset))
        currentStringOffset += len(name) + 1
    # Emit strings
    for function in functions:
        encoded = bytearray(function[1].encode())
        encoded.append(0)
        mapfile.write(encoded)
