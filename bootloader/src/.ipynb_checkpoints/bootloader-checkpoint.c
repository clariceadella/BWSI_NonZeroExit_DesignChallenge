


// Hardware Imports
#include "inc/hw_memmap.h" // Peripheral Base Addresses
#include "inc/lm3s6965.h" // Peripheral Bit Masks and Registers
#include "inc/hw_types.h" // Boolean type
#include "inc/hw_ints.h" // Interrupt numbers

// Driver API Imports
#include "driverlib/flash.h" // FLASH API
#include "driverlib/sysctl.h" // System control API (clock/reset)
#include "driverlib/interrupt.h" // Interrupt API

// Application Imports
#include "uart.h"
#include "bearssl.h"
#include "string.h"

// Forward Declarations
void load_initial_firmware(void);
void load_firmware(void);
void boot_firmware(void);
long program_flash(uint32_t, unsigned char*, unsigned int);


// Firmware Constants
#define METADATA_BASE 0xFC00  // base address of version and firmware size in Flash
#define FW_BASE 0x10000  // base address of firmware in Flash


// FLASH Constants
#define FLASH_PAGESIZE 1024
#define FLASH_WRITESIZE 4

// Protocol Constants
#define OK    ((unsigned char)0x00)
#define ERROR ((unsigned char)0x01)
#define UPDATE ((unsigned char)'U')
#define BOOT ((unsigned char)'B')


// Firmware v2 is embedded in bootloader
extern int _binary_firmware_bin_start;
extern int _binary_firmware_bin_size;


// Device metadata
uint16_t *fw_version_address = (uint16_t *) METADATA_BASE;
uint16_t *fw_size_address = (uint16_t *) (METADATA_BASE + 2);
uint8_t *fw_release_message_address;

// Firmware Buffer
unsigned char data[30*FLASH_PAGESIZE]={'\0'};
unsigned char datawithtag[30*FLASH_PAGESIZE]={'\0'};

//decryption variables
unsigned char key[16]=KEY;
unsigned char hmackey[32]=HMAC;
unsigned char iv[16]=IV;
unsigned char tag[16];

size_t key_len=16, cipher_len=FLASH_PAGESIZE, iv_len=16, hmackey_len=16;


br_aes_ct_ctr_keys bc;
br_gcm_context gc;
void PrintArr(char *arr, int length)
{
    for(int i=0;i<length;i++)
    {
        char byte=arr[i];
        byte=byte >> 4;
        if(byte<10)
            byte += 0x30;
        else
            byte += 0x37;
        uart_write(UART2, byte);
        
        byte=arr[i];
        byte=byte & 0xF;
        if(byte<10)
            byte += 0x30;
        else
            byte += 0x37;
        uart_write(UART2, byte);
    }
}
/*
InitializeAES initializes some bearssl structs for decryption
*/
void InitializeAES()
{
    //initialize a counter
	br_aes_ct_ctr_init(&bc,key,key_len);
    //initialize the gcm context
	br_gcm_init(&gc,&bc.vtable,br_ghash_ctmul32);
    br_gcm_reset(&gc, iv, iv_len);
}
 //hmac hashing code, taken from the slack channel
  
int HMACFunction(char* key, int key_len, char* data, int len, char* out) 
{    
    br_hmac_key_context kc;    
    br_hmac_context ctx;    
    br_hmac_key_init(&kc, &br_sha256_vtable, key, key_len);    
    br_hmac_init(&ctx, &kc, 0);  
    br_hmac_update(&ctx, data, len);    
    br_hmac_out(&ctx, out);  
        
    return 32;
}
/*
the decryptAesGCM function takes three parameters: data is the data to decrypt, length is the length of data, and
check_tag specifies whether we want a tag check. The function returns true if the tag matches, 0 otherwise
*/
int DecryptAesGCM(unsigned char *data, int length, int check_tag)
{
    
	//decrypt first
	br_gcm_run(&gc, 0, data, length);
    //returning the tag check
    if(check_tag)
    {
      
        return br_gcm_check_tag(&gc, tag);

    }
    else
        return 0;
}
void EncryptAesGCM(unsigned char *data, int length)
{
	//decrypt first
	br_gcm_run(&gc, 1, data, length);
    //returning the tag check
    br_gcm_get_tag(&gc, tag);
    uart_write_str(UART2, (char *)tag);
}
int main(void) {

  // Initialize UART channels
  // 0: Reset
  // 1: Host Connection
  // 2: Debug
  uart_init(UART0);
  uart_init(UART1);
  uart_init(UART2);

  InitializeAES();
  
 // Enable UART0 interrupt
  IntEnable(INT_UART0);
  IntMasterEnable();

 load_initial_firmware();

  uart_write_str(UART2, "Welcome to the BWSI Vehicle Update Service!\n");
  uart_write_str(UART2, "Send \"U\" to update, and \"B\" to run the firmware.\n");
  uart_write_str(UART2, "Writing 0x20 to UART0 will reset the device.\n");

  int resp;
  while (1){
    uint32_t instruction = uart_read(UART1, BLOCKING, &resp);
    if (instruction == UPDATE){
      uart_write_str(UART1, "U");
      load_firmware();
    } else if (instruction == BOOT){
      uart_write_str(UART1, "B");
      boot_firmware();
    }
  }
}


/*
 * Load initial firmware into flash
 */
void load_initial_firmware(void) {
  int size = (int)&_binary_firmware_bin_size;
  int *data = (int *)&_binary_firmware_bin_start;
    
  uint16_t version = 2;
  uint32_t metadata = (((uint16_t) size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) "This is the initial release message.";
    
  int i = 0;
  for (; i < size / FLASH_PAGESIZE; i++){
       program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), FLASH_PAGESIZE);
  }
  program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), size % FLASH_PAGESIZE);
}


/*
 * Load the firmware into flash.
 */
void load_firmware(void)
{
  int frame_length = 0;
  int read = 0;
  uint32_t rcv = 0;
  uint8_t rcv_8=0;
  uint32_t data_index = 0; //data_index is used to reference data array
  uint32_t page_addr = 0; //used for flashing 
  uint32_t version = 0;
  uint32_t size = 0;
    
  /*buffer only holds metadata!*/
  unsigned char buffer[16];
  /*the hmac that the bootloader receives from the bootloader*/
  unsigned char hmac[32];
  /* the hmac that the bootloader calculates to verify the hmac*/
  unsigned char calchmac[32];
  
/*
everything is sent in frames, even when the frame_length is not needed. The metadata, tag, and hmac frames are examples of having unneeded frame_length data, since their size is pre-determined. 

*/
    
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length = (int)rcv << 8;
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length |= (int)rcv;
  
  for(int i=0;i<16;i++)
  {
      rcv_8 = uart_read(UART1, BLOCKING, &read);
      hmac[i]=rcv_8;
  }
    
  //ackowledge hmac part 1 (first 16 bytes). There are two parts because the hmac is 32 bytes and each frame only comes in 16 bytes
  uart_write(UART1, OK);

  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length = (int)rcv << 8;
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length |= (int)rcv;

  for(int i=16;i<32;i++)
  {
      rcv_8 = uart_read(UART1, BLOCKING, &read);
      hmac[i]=rcv_8;
  }
 
  //ackowledge hmac part 2 (the second half of the 16 bytes)
  uart_write(UART1, OK);

  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length = (int)rcv << 8;
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length |= (int)rcv;  
    
  for(int i=0;i<16;i++)
  {
      rcv_8 = uart_read(UART1, BLOCKING, &read);
      tag[i]=rcv_8;
  }
      
  //ackowledge the tag
  uart_write(UART1, OK);
    
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length = (int)rcv << 8;
  rcv = uart_read(UART1, BLOCKING, &read);
  frame_length |= (int)rcv;
    
  for(int i=0;i<16;i++)
  {
      rcv_8 = uart_read(UART1, BLOCKING, &read);
      buffer[i]=rcv_8;
  }
    
  //ackowledge the metadata
  uart_write(UART1, OK);
    /*
  buffer holds the metadata. The data array holds the entire firmware, including the metadata, which is why a memcpy is used here to copy the contents of buffer to data.
  */
  memcpy(data,buffer,16);
   
  data_index += 16;//adding 16 to the data_index as the meta_data is 16 bytes. 
 
  /* Loop here until you can get all your characters and stuff */
  while (1) {
    uint32_t frame_length=0;
      
    //Get two bytes for the length.
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length = (int)rcv << 8;
    rcv = uart_read(UART1, BLOCKING, &read);
    frame_length |= (int)rcv;

/*
    if the frame length is 0, we're done here. Exit the while loop and proceed
    */
      
    if (frame_length == 0) {

        uart_write(UART1, OK);
        uart_write_str(UART2,"all frames received\n");
        nl(UART2);
        break;
    
    } // if
     //get the said number of bytes 
    for(int i=0;i<frame_length;i++)
    {
      rcv_8 = uart_read(UART1, BLOCKING, &read);

      data[data_index]=rcv_8;
      data_index ++;
    }     

    uart_write(UART1, OK); // Acknowledge the frame.
  } // while(1)

  HMACFunction(hmackey, 32, tag, 16, calchmac);

  //check HMAC using the memcmp method, as strcmp may factor in null bytes
  if(memcmp(hmac,calchmac,32))
  {
      uart_write_str(UART2, "HMAC authentication failed");
      return;
  }
    //decrypt the metadata
  DecryptAesGCM(buffer,16,0);
    
//resetting the counter for double decryption
  br_gcm_reset(&gc, iv, iv_len);
 
    
  // Get version.
  rcv = buffer[0];
  version = (uint32_t)rcv << 8;
  rcv = buffer[1];
  version |= (uint32_t)rcv;
  
  uart_write_str(UART2, "Received Firmware Version: ");
  uart_write_hex(UART2, version);
  nl(UART2);
  
  // Get size.
  rcv = buffer[2];
  size = (uint32_t)rcv << 8;
  rcv = buffer[3];
  size |= (uint32_t)rcv;
  

  uart_write_str(UART2, "Received Firmware Size: ");
  uart_write_hex(UART2, size);
  nl(UART2);
  

  // Compare to old version and abort if older (note special case for version 0).
  uint16_t old_version = *fw_version_address;

  if (version != 0 && version < old_version) {
    uart_write(UART1, ERROR); // Reject the metadata.
    SysCtlReset(); // Reset device
    return;
  } else if (version == 0) {
    // If debug firmware, don't change version
    version = old_version;
  }

  // Write new firmware size and version to Flash
  // Create 32 bit word for flash programming, version is at lower address, size is at higher address
  uint32_t metadata = ((size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) (FW_BASE + size);

  uart_write(UART1, OK); // Acknowledge the metadata.
   
  /*
  if the function returns true, meaning that the tag matches, proceed to flash the firmware. 
  */    
  if(DecryptAesGCM(data,data_index,1))
  {
      uart_write_str(UART2, "data authentication verified");
      unsigned char *flasher = data + 16;
      for(int i=0;i<data_index-16;i++)
      {
          if(i==FLASH_PAGESIZE*page_addr)
          {
             program_flash(page_addr*FLASH_PAGESIZE+FW_BASE, flasher+FLASH_PAGESIZE*page_addr, FLASH_PAGESIZE);
             page_addr++;
          } else if(i==data_index-1)
             program_flash(page_addr*FLASH_PAGESIZE+FW_BASE, flasher+FLASH_PAGESIZE*page_addr, data_index-page_addr*FLASH_PAGESIZE);

      }
      nl(UART2);
      uart_write_str(UART2,"all pages programmed");
      
  }else{
      uart_write_str(UART2, (char *)data);
      uart_write_str(UART2, "data authentication failed");
      return;
  }
  
}


/*
 * Program a stream of bytes to the flash.
 * This function takes the starting address of a 1KB page, a pointer to the
 * data to write, and the number of byets to write.
 *
 * This functions performs an erase of the specified flash page before writing
 * the data.
 */
long program_flash(uint32_t page_addr, unsigned char *data, unsigned int data_len)
{
  unsigned int padded_data_len;

  // Erase next FLASH page
  FlashErase(page_addr);

  // Clear potentially unused bytes in last word
  if (data_len % FLASH_WRITESIZE){
    // Get number unused
    int rem = data_len % FLASH_WRITESIZE;
    int i;
    // Set to 0
    for (i = 0; i < rem; i++){
      data[data_len-1-i] = 0x00;
    }
    // Pad to 4-byte word
    padded_data_len = data_len+(FLASH_WRITESIZE-rem);
  } else {
    padded_data_len = data_len;
  }

  // Write full buffer of 4-byte words
  return FlashProgram((unsigned long *)data, page_addr, padded_data_len);
}


void boot_firmware(void)
{
  uart_write_str(UART2, (char *) fw_release_message_address);

  // Boot the firmware
    __asm(
    "LDR R0,=0x10001\n\t"
    "BX R0\n\t"
  );
}

 

