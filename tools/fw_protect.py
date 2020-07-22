"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct
from Crypto.Util.Padding import pad, unpad

#open a file with the keys
def protect_firmware(infile, outfile, version, message):
    #read the file with the key-- fix later
    with open("secretbuildoutput.txt", 'rb') as k:
        key1 = k.read(16)
        iv = k.read(16)
    
    #Will encrypt with AES GCM mode
    cipher_encrypt = AES.new(key1, AES.MODE_GCM, IV=iv)
    
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()
    
    #Pack version and size into two little-endian shorts
    #add padding!
    metadata = struct.pack('<HH', version, len(firmware))
    metadata = pad(metadata, 16)
    
    # Append null-terminated message to end of firmware
    firmware_and_message = firmware + message.encode() + b'\00'

    #dealing with the the main data
    cipher_encrypt.update(metadata)
    ciphertext, tag = cipher_encrypt.encrypt_and_digest(firmware_and_message)
    
    # Write the encrypted message to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(ciphertext)
    
    #not needed but helpful
    print("Send this info: ")
    print("Nonce: ".encode("utf-8") + IV)
    print("Metadata:".encode("utf-8") + meta)
    print("Ciphertext: ".encode("utf-8") + ciphertext)
    print("Tag: ".encode("utf-8") + tag)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
