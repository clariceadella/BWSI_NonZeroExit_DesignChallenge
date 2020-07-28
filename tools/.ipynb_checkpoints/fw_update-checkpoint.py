#!/usr/bin/env python
#
# UPDATE TOOL DIAGRAM
#
#                                                     
#       HMAC          Tag          (Metadata   Padding)       Firmware 
# ================================================================================
# |   32 Bytes    |  16 Bytes    | ( 4 Bytes  12 Bytes)  |    16 Bytes    | cont...
# =================================================================================
#
# Every 16 Bytes of data is packed with the length of that data. 
# Therefore, the updater tool sends the bootloader 18 Byte frames: 
#
#     Length          Data
# ============================
# |   2 Bytes   |  16 Bytes  |
# ============================
#
# We write a frame to the bootloader, then wait for it to respond with an
# OK message so we can write the next frame. The OK message in this case is
# just a zero

import argparse
import struct
import time

from serial import Serial

RESP_OK = b'\x00'
FRAME_SIZE = 16

def send_frame(ser, frame, debug=False):
    ser.write(frame)  # Write the frame...

    if debug:
        print(frame)

    resp = ser.read()  # Wait for an OK from the bootloader

    time.sleep(0.1)

    if resp != RESP_OK: #if there is no OK --> ERROR
        raise RuntimeError("ERROR: Bootloader responded with {}".format(repr(resp)))

    if debug:
        print("Resp: {}".format(ord(resp)))


def main(ser, infile, debug):
    ser.write(b'U')  
    print('Waiting for bootloader to enter update mode...')
    if ser.read(1).decode() != 'U':
        return
    print("sending data")
    # Open serial port. Set baudrate to 115200. Set timeout to 2 seconds.
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    for idx, frame_start in enumerate(range(0, len(firmware), FRAME_SIZE)):
        data = firmware[frame_start: frame_start + FRAME_SIZE] #16 byte frames of data

        # Get length of data.
        length = len(data)
        frame_fmt = '>H{}s'.format(length)

        # Construct frame.
        frame = struct.pack(frame_fmt, length, data) 

        if debug:
            print("Writing frame {} ({} bytes)...".format(idx, len(frame)))

        send_frame(ser, frame, debug=debug) #frames include: 2 bytes (length) and 16 bytes (data)

    print("Done writing firmware.")

    # Send a zero length payload to tell the bootlader to finish writing it's page.
    ser.write(struct.pack('>H', 0x0000))

    return ser


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')

    parser.add_argument("--port", help="Serial port to send update over.",
                        required=True)
    parser.add_argument("--firmware", help="Path to firmware image to load.",
                        required=True)
    parser.add_argument("--debug", help="Enable debugging messages.",
                        action='store_true')
    args = parser.parse_args()

    print('Opening serial port...')
    ser = Serial(args.port, baudrate=115200, timeout=2)
    main(ser=ser, infile=args.firmware, debug=args.debug)


