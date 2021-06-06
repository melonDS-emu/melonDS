#!/usr/bin/env python3

import sys
import socket
import struct

for UDP_IP in ["192.168.1.158", "127.0.0.1"]:
    UDP_PORT = 8888
    MESSAGE = struct.pack('<H', 0xD592)

    for i in range(1, len(sys.argv)):
        inst = sys.argv[i]
        if inst == "r":
            MESSAGE += struct.pack('<H', 0)
            continue
        elif inst == "w":
            MESSAGE += struct.pack('<H', 1)
            continue
        elif inst == "hr":
            MESSAGE += struct.pack('<H', 2)
            continue
        elif inst == "hw":
            MESSAGE += struct.pack('<H', 3)
            continue
        MESSAGE += struct.pack('<H', int(inst, 16))

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))
