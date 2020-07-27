"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct
import pathlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad
from Crypto.Hash import HMAC, SHA256

FILE_DIR = pathlib.Path(__file__).parent.absolute()
bootloader = FILE_DIR / '..' / 'bootloader'

def protect_firmware(infile, outfile, version, message):
    
    #Read the file with the key and iv
    with open(bootloader/"secret_build_output.txt", 'rb') as k:
        key1 = k.read(16)
        iv = k.read(16)
        hmackey1 = k.read(16)
    
    #Encrypt with AES GCM mode
    cipher_encrypt = AES.new(key1, AES.MODE_GCM, nonce=iv)
    
    #Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()
    
    #Pack version and size into two little-endian shorts, pad the metadata
    metadata = struct.pack('>HH', version, len(firmware))
    print("the size is"+str(len(firmware)))
    metadata = pad(metadata, 16)
    
    #Append metadata and null-terminated message to end of firmware
    firmware_and_message = metadata + firmware + message.encode()

    #Append the metadata and firmware together, add a tag 
    #cipher_encrypt.update(metadata)
    #cipher_encrypt.update(firmware_and_message)
    ciphertext, tag = cipher_encrypt.encrypt_and_digest(firmware_and_message)
    
    #new HMAC stuff
    new = tag + ciphertext
    h = HMAC.new(hmackey1, len(new), digestmod=SHA128)
    
    #Write the encrypted data to outfile, tag and ciphertext will be in the same file
    with open(outfile, 'wb+') as outfile:
        outfile.write(h.digest())
        outfile.write(tag)
        outfile.write(ciphertext)
    
    #Not needed but helpful, prints the values of everything
    print("Send this info: ")
    print("Nonce: ".encode("utf-8") + iv)
    print("Metadata:".encode("utf-8") + metadata)
    print("Tag: ".encode("utf-8") + tag)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
