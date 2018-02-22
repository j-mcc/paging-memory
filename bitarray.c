/*
 * $Author: o1-mccune $
 * $Date: 2017/12/02 05:48:41 $
 * $Revision: 1.4 $
 * $Log: bitarray.c,v $
 * Revision 1.4  2017/12/02 05:48:41  o1-mccune
 * removed print statement.
 *
 * Revision 1.3  2017/12/01 20:06:44  o1-mccune
 * Added helper functions
 *
 * Revision 1.2  2017/11/26 21:50:02  o1-mccune
 * Updated bitarray functions to a reentrant style
 *
 * Revision 1.1  2017/11/19 18:54:54  o1-mccune
 * Initial revision
 *
 */


/* Created with help from 'The Bit Twiddler' http://bits.stephan-brumme.com/basics.html */

#include "bitarray.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

//static unsigned char *bitVector = NULL;
//static double bytes = 0;
//static unsigned int slots;

void clearAll(bitarray_t *bitArray){
  int i;
  for(i = 0; i < bitArray->bytes; i++) bitArray->bits[i] = MIN_VALUE;
}
/*
void clearAll(){
  int i;
  for(i = 0; i < bytes; i++) bitVector[i] = MIN_VALUE;
}
*/
void setAll(bitarray_t *bitArray){
  int i;
  for(i = 0; i < bitArray->bytes; i++) bitArray->bits[i] = MAX_VALUE;
}
/*
void setAll(){
  int i;
  for(i = 0; i < bytes; i++) bitVector[i] = MAX_VALUE;
}

void printByte(unsigned char byte){
  int i;
  for(i = 0; i < CHAR_BIT; i++) fprintf(stderr, "%d", getBit(i));
}

int initBitVector(unsigned int size, unsigned char initialValue){
  if(size <= 0) return -1;
  slots = size;
  bytes = ceil((size * 1.)/(CHAR_BIT * 1.));
  if((bitVector = malloc(bytes)) == NULL) return -1;
  if(initialValue == MIN_VALUE) clearAll();
  if(initialValue == MAX_VALUE) setAll();
  return 0;
}
*/
int initBitVector(bitarray_t *bitArray, unsigned int size, unsigned char initialValue){
  if(size <= 0) return -1;
  bitArray->slots = size;
  bitArray->bytes = ceil((size * 1.)/(CHAR_BIT * 1.));
  if((bitArray->bits = malloc(bitArray->bytes)) == NULL) return -1;
  if(initialValue == MIN_VALUE) clearAll(bitArray);
  if(initialValue == MAX_VALUE) setAll(bitArray);
  return 0;
}

int isFull(bitarray_t *bitArray){
  int i;
  for(i = 0; i < bitArray->slots; i++)
    if(!getBit(bitArray, i)) return 0;
  return 1;
}
/*
int isFull(){
  int i;
  for(i = 0; i < slots; i++){
    if(!getBit(i)) return 0;
  }
  return 1;
}
*/
int isEmpty(bitarray_t *bitArray){
  int i;
  for( i = 0; i < bitArray->slots; i++)
    if(getBit(bitArray, i)) return 0;
  return 1;
}
/*
int isEmpty(){
  int i;
  for(i = 0; i < slots; i++){
    if(getBit(i)) return 0;
  }
  return 1;
}

void freeBitVector(){
  if(bitVector) free(bitVector);
}
*/
void freeBitArray(bitarray_t *bitArray){
  if(bitArray)
    if(bitArray->bits){
      free(bitArray->bits);
    }
}

int getBit(bitarray_t *bitArray, unsigned int index){
  if(index > (CHAR_BIT * bitArray->bytes - 1)) return -1;  //index out of range
  unsigned int byte = floor(index / CHAR_BIT);
  unsigned char x = bitArray->bits[byte];
  x >>= (index % CHAR_BIT);
  return (x & 1) != 0;
}
/*
int getBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned int byte = floor(index / CHAR_BIT);
  unsigned char x = bitVector[byte];
  x >>= (index % CHAR_BIT);
  return (x & 1) != 0;
}
*/
int setBit(bitarray_t *bitArray, unsigned int index){
  if(index > (CHAR_BIT * bitArray->bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1 << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitArray->bits[byte] |= mask;
  return 0;  
}
/*
int setBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1 << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] |= mask;
  return 0;  
}
*/
int flipBit(bitarray_t *bitArray, unsigned int index){
  if(index > (CHAR_BIT * bitArray->bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitArray->bits[byte] ^= mask;
  return 0;
}
/*
int flipBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] ^= mask;
  return 0;
}
*/
int clearBit(bitarray_t *bitArray, unsigned int index){
  if(index > (CHAR_BIT * bitArray->bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitArray->bits[byte] &= ~mask;
  return 0;
}
/*
int clearBit(unsigned int index){
  if(index > (CHAR_BIT * bytes - 1)) return -1;  //index out of range
  unsigned char mask = 1U << (index % CHAR_BIT);
  unsigned int byte = floor(index / CHAR_BIT);
  bitVector[byte] &= ~mask;
  return 0;
}
*/
void printBitVector(bitarray_t *bitArray){
  int i;
  for(i = 0; i < (CHAR_BIT * bitArray->bytes); i++){
    if(i % CHAR_BIT == 0) fprintf(stderr, " ");
    fprintf(stderr, "%d", getBit(bitArray, i));
  }
  fprintf(stderr, "\n");
}

int indexOfNextSetBit(bitarray_t *bitArray){
  int i;
  for(i = 0; i < bitArray->slots; i++){
    if(getBit(bitArray, i)) return i;
  }
}

int getNumberSetBits(bitarray_t *bitArray){
  int numSet = 0;
  int byte, bit;
  for(byte = 0; byte < bitArray->bytes; byte++){
    for(bit = 0; bit < CHAR_BIT; bit++){
      if(getBit(bitArray, (CHAR_BIT * byte) + bit)) numSet++;
    }
  }
  return numSet;
}
/*
void printBitVector(){
  int i;
  for(i = 0; i < (CHAR_BIT * bytes); i++){
    if(i % CHAR_BIT == 0) fprintf(stderr, " ");
    fprintf(stderr, "%d", getBit(i));
  }
  fprintf(stderr, "\n");
}
*/

/*
int main(){
  initVector(3, MIN_VALUE);
  printArray();
  if(setBit(4) == -1) fprintf(stderr, "Setting bit 4 failed\n");
  if(setBit(0) == -1) fprintf(stderr, "Setting bit 0 failed\n");
  if(setBit(14) == -1) fprintf(stderr, "Setting bit 14 failed\n");
  printArray();
  return 0;
}
*/
