# BWSI Embedded Security & Hardware Hacking Design Challenge
We designed a secure firmware distribution system using encryption protocols such as AES in GCM mode and HMAC.

## Requirements
* Python 3
* C
* BearSSL


## Installation
1. Install the requirements above
2. Clone the repostiroy

## Running the Program
1. Navigate to the `design-challenge-non-zero-exit` folder
2. Run the following
`python3 tools/bl_build.py`
`python3 tools/fw_protect.py --infile firmware/firmware/gcc/main.bin --outfile firmwareblob.blob --version 3 --message "test"`
`python3 tools/bl_emulate.py`
`python3 tools/fw_update.py --port /embsec/UART1 --firmware firmwareblob.blob`

## Design Overview

Our design first generates a 16-byte key and IV in the bootloader build tool. This key is sent to the bootloader and the secret_build_output.txt file. The key is used in encrypting the raw firmware data using AES in GCM mode. A tag is generated and the firmware is sent to the bootloader in 18-Byte frames. The bootloader reads the tags and decrypts the data using the tags and the key

###  bl_build.py

This tool generates a 16 byte key and 16 byte IV, both of which are used in the AES encryption. The bootloader is also built in this tool.

### fw_protect.py

This tool sends the metadata, firmware, and message to the fw_update.py tool, encrypted under AES in GCM mode. 

### fw_update.py

The firmware loads the tag, the metadata, and then the firmware into the bootloader. The firmware is loaded into the firmware in 18 byte frames, with the first two bytes being a short indicating the length of the data.