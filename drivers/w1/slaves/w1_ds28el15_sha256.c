
/*
 * 1-Wire SHA256 software implementation for the ds23el15 chip
 *
 * Copyright (C) 2013 maximintergrated
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


#include <linux/string.h>
#include <linux/memory.h>

#include "w1_ds28el15_sha256.h"

#define SHA_256_INITIAL_LENGTH    8

#define TRUE 1
#define FALSE 0


// SHA-256 Functions
int compute_sha256(uchar* message, short length, ushort skipconst, ushort reverse, uchar* digest);
int compute_mac256(uchar* message, short length, uchar* MAC);
int verify_mac256(uchar* message, short length, uchar* compare_MAC);
int calculate_nextsecret256(uchar* binding, uchar* partial, int page_num, uchar* manid);
void set_secret(uchar *secret_data);
void set_romid(uchar *romid_data);


// Utility Functions
ulong sha_ch(ulong x, ulong y, ulong z);
ulong sha_maj(ulong x, ulong y, ulong z);
ulong sha_rotr_32(ulong val, ushort r);
ulong sha_shr_32(ulong val, ushort r);
ulong sha_bigsigma256_0(ulong x);
ulong sha_littlesigma256_0(ulong x);
ulong sha_littlesigma256_1(ulong x);
void sha_copy32(ulong* p1, ulong* p2, ushort length);
void sha_copyWordsToBytes32(ulong* input, uchar* output, ushort numwords);
void sha_writeResult(ushort reverse, uchar* outpointer);
ulong sha_getW(int index);
void sha_prepareSchedule(uchar* message);
void sha256_hashblock(uchar* message, ushort lastblock);

// hold secret for creating a 
static uchar secret[32];

uchar rom_no[8];

// SHA-256 globals values
ulong SHA_256_Initial[] = 
{
   0x6a09e667,
   0xbb67ae85,
   0x3c6ef372,
   0xa54ff53a,
   0x510e527f,
   0x9b05688c,
   0x1f83d9ab,
   0x5be0cd19
};

ulong SHA_CONSTANTS[] =  
{
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
  0xca273ece, 0xd186b8c7, 0xeada7dd6, 0xf57d4f7f, 0x06f067aa, 0x0a637dc5, 0x113f9804, 0x1b710b35,
  0x28db77f5, 0x32caab7b, 0x3c9ebe0a, 0x431d67c4, 0x4cc5d4be, 0x597f299c, 0x5fcb6fab, 0x6c44198c
};

uchar workbuffer[128];

ulong a32, b32, c32, d32, e32, f32, g32, h32; // SHA working variables
ulong W32[16];                                // SHA message schedule
ulong H32[8];                                 // last SHA result variables
 
int reverse_endian=1; 
int sha_debug=0;

//----------------------------------------------------------------------
// Set the Secret to be used for calculating MAC's in the ComputeMAC
// function.
//
void set_secret(uchar *secret_data)
{ 
   int i;

   for (i = 0; i < 32; i++)
      secret[i] = secret_data[i];   
}

//----------------------------------------------------------------------
// Set the Secret to be used for calculating MAC's in the ComputeMAC
// function.
//
void set_romid(uchar *romid_data)
{ 
   int i;

   for (i = 0; i < 8; i++)
      rom_no[i] = romid_data[i];   
}


//----------------------------------------------------------------------
// Computes SHA-256 given the MT digest buffer after first inserting
// the secret at the correct location in the array defined by all Maxim
// devices. Since part of the input is secret it is called a Message
// Autnentication Code (MAC).
//
// 'MT'       - buffer containing the message digest
// 'length'   - Length of block to digest
// 'MAC'      - result MAC in byte order used by 1-Wire device
//
int compute_mac256(uchar* MT, short length, uchar* MAC)
{
   int i,j;  
   uchar tmp[4]; 

   // check for two block format
   if (length == 119)
   {
      // insert secret
      memcpy(&MT[64], &secret[0], 32);

      // change to little endian for A1 devices
      if (reverse_endian)
      {
         for (i = 0; i < 108; i+=4)
         {
            for (j = 0; j < 4; j++)
               tmp[3 - j] = MT[i + j];
      
            for (j = 0; j < 4; j++)
               MT[i + j] = tmp[j];
         }
      }
   
      compute_sha256(MT, 119, TRUE, TRUE, MAC);
   }
   // one block format
   else
   {
      // insert secret
      memcpy(&MT[0], &secret[0], 32);

      // change to little endian for A1 devices
      if (reverse_endian)
      {
         for (i = 0; i < 56; i+=4)
         {
            for (j = 0; j < 4; j++)
               tmp[3 - j] = MT[i + j];
      
            for (j = 0; j < 4; j++)
               MT[i + j] = tmp[j];
         }
      }

      compute_sha256(MT, 55, TRUE, TRUE, MAC);
   }

   return TRUE;
}

//----------------------------------------------------------------------
// Computes SHA-256 given the MT digest buffer after first iserting
// the secret at the correct location in the array defined by all Maxim
// devices. Since part of the input is secret it is called a Message
// Autnentication Code (MAC).
//
// 'MT'           - buffer containing the message digest
// 'length'       - Length of block to digest
// 'compare_MAC'  - MAC in byte order used by 1-Wire device to compare
//                 with calculate MAC.
//
int verify_mac256(uchar* MT, short length, uchar* compare_MAC)
{
	uchar calc_mac[32];
	int i;

	// calculate the MAC
	compute_mac256(MT, length, calc_mac);

	// Compare calculated mac with one read from device
	for (i = 0; i < 32; i++)
	{
		if (compare_MAC[i] != calc_mac[i])
			 return FALSE;
	}
	return TRUE;
}

//----------------------------------------------------------------------
// Performs a Compute Next SHA-256 calculation given the provided 32-bytes
// of binding data and 8 byte partial secret. The first 8 bytes of the
// resulting MAC is set as the new secret.
// 
// 'binding'  - 32 byte buffer containing the binding data
// 'partial'  - 8 byte buffer with new partial secret
// 'page_num'  - page number that the compute is calculated on
// 'manid'  - manufacturer ID
//
// Globals used : secret used in calculation and set to new secret
//
// Returns: TRUE if compute successful
//          FALSE failed to do compute
//
int calculate_nextsecret256(uchar* binding, uchar* partial, int page_num, uchar* manid)
{
   uchar MT[128];
   uchar MAC[64];

   // clear 
   memset(MT,0,128);

   // insert page data
   memcpy(&MT[0],binding,32);

   // insert challenge
   memcpy(&MT[32],partial,32);

   // insert ROM number or FF
   memcpy(&MT[96],rom_no,8);

   MT[106] = page_num;
   MT[105] = manid[0];
   MT[104] = manid[1];

   compute_mac256(MT, 119, MAC);

   // set the new secret to the first 32 bytes of MAC
   set_secret(MAC);

   return TRUE;
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_ch(ulong x, ulong y, ulong z)
{
   return (x & y) ^ ((~x) & z);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_maj(ulong x, ulong y, ulong z)
{
   ulong temp = x & y;
   temp ^= (x & z);
   temp ^= (y & z);
   return temp;  //(x & y) ^ (x & z) ^ (y & z);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_rotr_32(ulong val, ushort r)
{
   val = val & 0xFFFFFFFFL;
   return ((val >> r) | (val << (32 - r))) & 0xFFFFFFFFL;
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_shr_32(ulong val, ushort r)
{
   val = val & 0xFFFFFFFFL;
   return val >> r;
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_bigsigma256_0(ulong x)
{
   return sha_rotr_32(x,2) ^ sha_rotr_32(x,13) ^ sha_rotr_32(x,22);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_bigsigma256_1(ulong x)
{
   return sha_rotr_32(x,6) ^ sha_rotr_32(x,11) ^ sha_rotr_32(x,25);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_littlesigma256_0(ulong x)
{
   return sha_rotr_32(x,7) ^ sha_rotr_32(x,18) ^ sha_shr_32(x,3);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_littlesigma256_1(ulong x)
{
   return sha_rotr_32(x,17) ^ sha_rotr_32(x,19) ^ sha_shr_32(x,10);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
void sha_copy32(ulong* p1, ulong* p2, ushort length)
{
   while (length > 0)
   {
      *p2++ = *p1++;
      length--;
   }
}

//----------------------------------------------------------------------
// SHA-256 support function
//
void sha_copyWordsToBytes32(ulong* input, uchar* output, ushort numwords)
{
    ulong temp;
    ushort i;

    for (i=0;i<numwords;i++)
    {
        temp = *input++;
        *output++ = (uchar)(temp >> 24);
        *output++ = (uchar)(temp >> 16);
        *output++ = (uchar)(temp >> 8);
        *output++ = (uchar)(temp);
    }
}

//----------------------------------------------------------------------
// SHA-256 support function
//
void sha_writeResult(ushort reverse, uchar* outpointer)
{
   int i;
   uchar tmp;

   sha_copyWordsToBytes32(H32, outpointer, 8); 

   if (reverse)
   {
      for (i = 0; i < 16; i++)
      {  
         tmp = outpointer[i];
         outpointer[i] = outpointer[31-i];
         outpointer[31-i] = tmp;
      }
   }

}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong sha_getW(int index)
{
   ulong newW;
   if (index < 16)
   {
      return W32[index];
   }

   newW = sha_littlesigma256_1(W32[(index-2)&0x0f]) + 
            W32[(index-7)&0x0f] + 
          sha_littlesigma256_0(W32[(index-15)&0x0f]) + 
            W32[(index-16)&0x0f];
   W32[index & 0x0f] = newW & 0xFFFFFFFFL;  // just in case...

   return newW;
}

//----------------------------------------------------------------------
// Prepair the block for hashing
//
void sha_prepareSchedule(uchar* message)
{
   // we need to copy the initial message into the 16 W registers
   ushort i,j;
   ulong temp;
   for (i = 0; i < 16; i++)
   {
      temp = 0;
      for (j = 0; j < 4;j++)
      {
         temp = temp << 8;
         temp = temp | (*message & 0xff);
         message++;
      }

      W32[i] = temp;
   }
}

//----------------------------------------------------------------------
// Hash a single block of data. 
//
void sha256_hashblock(uchar* message, ushort lastblock)
{
   ushort sha1counter = 0;
   ushort sha1functionselect = 0;
   ushort i;
   ulong nodeT1, nodeT2;

   ulong Wt, Kt;

   // chunk the original message into the working schedule
   sha_prepareSchedule(message);

   a32 = H32[0];
   b32 = H32[1];
   c32 = H32[2];
   d32 = H32[3];
   e32 = H32[4];
   f32 = H32[5];
   g32 = H32[6];
   h32 = H32[7];

   // rounds
   for (i = 0; i < 64; i++)
   {
      Wt = sha_getW(i);
      Kt = SHA_CONSTANTS[i]; 

      nodeT1 = (h32 + sha_bigsigma256_1(e32) + sha_ch(e32,f32,g32) + Kt + Wt); // & 0xFFFFFFFFL;
      nodeT2 = (sha_bigsigma256_0(a32) + sha_maj(a32,b32,c32)); // & 0xFFFFFFFFL;
      h32 = g32;
      g32 = f32;
      f32 = e32;
      e32 = d32 + nodeT1;
      d32 = c32;
      c32 = b32;
      b32 = a32;
      a32 = nodeT1 + nodeT2;

      sha1counter++;
      if (sha1counter==20)
      {
         sha1functionselect++;
         sha1counter = 0;
      }			

   }

   if (!lastblock)
   {
      // now fix up our H array
      H32[0] += a32;
      H32[1] += b32;
      H32[2] += c32;
      H32[3] += d32;
      H32[4] += e32;
      H32[5] += f32;
      H32[6] += g32;
      H32[7] += h32;
   }
   else
   {
      // now fix up our H array
      H32[0] = a32;
      H32[1] = b32;
      H32[2] = c32;
      H32[3] = d32;
      H32[4] = e32;
      H32[5] = f32;
      H32[6] = g32;
      H32[7] = h32;
   }
}

//----------------------------------------------------------------------
// Computes SHA-256 given the data block 'message' with no padding. 
// The result is returned in 'digest'.   
//
// 'message'  - buffer containing the message 
// 'skipconst' - skip adding constant on last block (skipconst=1)
// 'reverse' - reverse order of digest (reverse=1, MSWord first, LSByte first)
// 'digest'   - result hash digest in byte order used by 1-Wire device
//
int compute_sha256(uchar* message, short length, ushort skipconst, ushort reverse, uchar* digest)
{
   ushort bytes_per_block;
   ushort nonpaddedlength;
   ushort numblocks;
   ushort i,j;
   ulong bitlength;
   ushort markerwritten;
   ushort lastblock;

   ushort wordsize = 32;

   if (sha_debug)
   {
      printk(KERN_INFO "\nSHA-256 INPUT:\n");
      for (i = 0; i < length; i+=4)
      {
         for (j = 0; j < 4; j++)
            printk(KERN_INFO "%02X ",message[i+j]);
         printk(KERN_INFO "\n");
      }
      printk(KERN_INFO "\n");
   }

   // if wordsize is 32 bits, we need 512 bit blocks.  else 1024 bit blocks.
   // that means 16 words are in one message.
   bytes_per_block = 16 * (wordsize / 8);
   // 1 byte for the '80' that follows the message, 8 or 16 bytes of length
   nonpaddedlength = length + 1 + (wordsize/4);
   numblocks = nonpaddedlength / bytes_per_block;
   if ((nonpaddedlength % bytes_per_block) != 0) 
   {
      // then there is some remainder we need to pad
      numblocks++;
   }

   sha_copy32(SHA_256_Initial, H32, SHA_256_INITIAL_LENGTH); 

   bitlength = 8 * length;
   markerwritten = 0;
   // 'length' is our number of bytes remaining.
   for (i = 0; i < numblocks; i++)
   {
      if (length > bytes_per_block)
      {
         memcpy(workbuffer, message, bytes_per_block);
         length -= bytes_per_block;
      }
      else if (length==bytes_per_block)
      {
         memcpy(workbuffer, message, length);
         length = 0;
      }
      else // length is less than number of bytes in a block
      {
         memcpy(workbuffer, message, length);
         // message is now used for temporary space
         message = workbuffer + length;     
         if (markerwritten == 0)
         {
            *message++ = 0x80;
            length++;
         }

         while (length < bytes_per_block)
         {
            // this loop is inserting padding, in this case all zeroes
            *message++ = 0;
            length++;
         }
         length = 0;
         // signify that we have already written the 80h
         markerwritten = 1;
      }

      // on the last block, put the bit length at the very end
      lastblock = (i == (numblocks - 1));
      if (lastblock)
      {
         // point at the last byte in the block
         message = workbuffer + bytes_per_block - 1;
         for (j = 0; j < wordsize/4; j++)
         {
            *message-- = (uchar)bitlength;
            bitlength = bitlength >> 8;
         }
      }

      // SHA in software 
      sha256_hashblock(workbuffer, (ushort)(lastblock && skipconst));
      message += bytes_per_block;
   }

   sha_writeResult(reverse, digest);

   if (sha_debug)
   {
      printk(KERN_INFO "\nSHA-256 Result:\n");
      for (i = 0; i < 32; i++)
         printk(KERN_INFO "%02X ",digest[i]);
      printk(KERN_INFO "\n");
   }

   return TRUE;
}

