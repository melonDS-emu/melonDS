#!/usr/bin/env python3

import sys
import socket
import struct

for UDP_IP in ["192.168.1.158", "127.0.0.1"]:
    UDP_PORT = 8888
    MESSAGE = struct.pack('<H', 0xD590)

    for i in range(1, len(sys.argv)):
        if sys.argv[i] == '-1':
            MESSAGE = struct.pack('<H', 0xD591)
            continue
        inst = sys.argv[i]
        splitted = inst.split('+')
        code = int(splitted[0], 16)
        for param in splitted[1:]:
            v, p = param.split('@')
            code += int(v) << int(p)
        MESSAGE += struct.pack('<H', code)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))
