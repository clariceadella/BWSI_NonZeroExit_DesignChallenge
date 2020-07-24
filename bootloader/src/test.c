#define KEY "hellohellohelloh"
#define IV "0123456789abcdef"
#define TAG {0xf0,0x5a,0xac,0x37,0x3e,0x43,0xb2,0xf0,0xf7,0x8e,0x2b,0x7c,0xac,0x4f,0x7,0x18}
#include "/lib/BearSSL/inc/bearssl.h"

unsigned char data[27]={0x7e,0x94,0x38,0xcc,0xed,0xc5,0x8a,0x79,0xf6,0x9e,0x1c,0x8a,0x6d,0xb1,0xd5,0x76,0xbe,0xb5,0xdd,0xe4,0xb1,0xc2,0xa4,0x9b,0xb5,0x69,0xe7,0xc2,0xc9,0x92,0xfd,0xe8,0x38,0x81,0x88}

//decryption variables
unsigned char key[16]=KEY;
unsigned char iv[16]=IV;
unsigned char tag[100]=TAG;

size_t key_len, cipher_len, iv_len;
key_len=16;
cipher_len=FLASH_PAGESIZE;
iv_len=16;

br_aes_ct_ctr_keys bc;
br_gcm_context gc;
void InitializeAES()
{
    //initialize a counter
	br_aes_ct_ctr_init(&bc,key,key_len);
    //initialize the gcm context
	br_gcm_init(&gc,&bc.vtable,br_ghash_ctmul32);

}
int DecryptAesGCM(char *data)
{
	//decrypt first
	br_gcm_reset(&gc, iv, iv_len);
	br_gcm_run(&gc, 0, data, FLASH_PAGESIZE);
    //return thhe tag comparison
	return br_gcm_check_tag(&gc, tag);		
}
void main()
{
    printf("the result is %s",data)
}