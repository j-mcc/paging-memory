/*
 * $Author: o1-mccune $
 * $Date: 2017/12/01 20:07:03 $
 * $Revision: 1.3 $
 * $Log: bitarray.h,v $
 * Revision 1.3  2017/12/01 20:07:03  o1-mccune
 * added helper function prototypes
 *
 * Revision 1.2  2017/11/26 21:50:55  o1-mccune
 * Updated bitarray function to a reentrant style
 *
 * Revision 1.1  2017/11/19 18:56:20  o1-mccune
 * Initial revision
 *
 */

#ifndef BITARRAY_H
#define BITARRAY_H

#define MAX_VALUE 255
#define MIN_VALUE 0

typedef struct{
  unsigned char *bits;
  unsigned int slots;
  unsigned int bytes;
}bitarray_t;

void clearAll(bitarray_t *bitArray);

//void clearAll();

void setAll(bitarray_t *bitArray);

//void setAll();

//void printByte(unsigned char byte);

//int initBitVector(unsigned int size, unsigned char initialValue);

int initBitVector(bitarray_t *bitArray, unsigned int size, unsigned char initialValue);

int getBit(bitarray_t *bitArray, unsigned int index);

//int getBit(unsigned int index);

int setBit(bitarray_t *bitArray, unsigned int index);

//int setBit(unsigned int index);

int flipBit(bitarray_t *bitArray, unsigned int index);

//int flipBit(unsigned int index);

int clearBit(bitarray_t *bitArray, unsigned int index);

//int clearBit(unsigned int index);

void printBitVector(bitarray_t *bitArray);

//void printBitVector();

void freeBitArray(bitarray_t *bitArray);

//void freeBitVector();

int isFull(bitarray_t *bitArray);

//int isFull();

int isEmpty(bitarray_t *bitArray);

//int isEmpty();

int indexOfNextSetBit(bitarray_t *bitArray);

int getNumberSetBits(bitarray_t *bitArray);
#endif
