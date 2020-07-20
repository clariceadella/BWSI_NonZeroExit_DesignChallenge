"""
Firmware Bundle-and-Protect Tool

"""
import argparse
import struct


def protect_firmware(infile, outfile, version, message):
    #need to somehow pass the key in
    key1 = b'key'
    
    #Will encrypt with AES GCM mode
    cipher_encrypt = AES.new(key1, AES.MODE_GCM, nonce=nonce)
    
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    # Append null-terminated message to end of firmware
    firmware_and_message = firmware + message.encode() + b'\00'

    #dealing with the the main data
    ciphertext, tag = cipher_encrypt.encrypt_and_digest(data)
    
    # Pack version and size into two little-endian shorts
    metadata = struct.pack('<HH', version, len(firmware))

    #updating with the metadata
    cipher_encrypt.update(metadata)
    
    # Append firmware and message to metadata
    firmware_blob = metadata + firmware_and_message

    # Write firmware blob to outfile
    with open(outfile, 'wb+') as outfile:
        outfile.write(firmware_blob)
    
    nonce = cipher_enc.nonce
    print("Send this info: ")
    print("Nonce: ".encode("utf-8") + nonce)
    print("Metadata:".encode("utf-8") + meta)
    print("Ciphertext: ".encode("utf-8") + ciphertext)
    print("Tag: ".encode("utf-8") + tag)
    
    cipher_decrypt = AES.new(key1, AES.MODE_GCM, nonce=nonce)
    cipher_decrypt.update(meta)
    plaintext = cipher_decrypt.decrypt_and_verify(ciphertext, tag)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile", help="Path to the firmware image to protect.", required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.", required=True)
    parser.add_argument("--version", help="Version number of this firmware.", required=True)
    parser.add_argument("--message", help="Release message for this firmware.", required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile, outfile=args.outfile, version=int(args.version), message=args.message)
