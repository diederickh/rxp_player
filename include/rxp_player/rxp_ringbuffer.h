/*

  rxp_ringbuffer
  --------------
  
  Writes data into a circular ring and read from it. When someone
  tries to write 100 bytes and there is only 10 bytes till the end 
  of the buffer, we will write 10 to the end and 90 bytes from the 
  start. All other operations just write/read into/fram this bufer.

 */
#ifndef RXP_RINGBUFFER_H
#define RXP_RINGBUFFER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef struct rxp_ringbuffer rxp_ringbuffer;

struct rxp_ringbuffer {
  uint8_t* buffer;                                                         /* the buffer we allocated */
  uint32_t head;                                                           /* the position in the buffer where we start writing */ 
  uint32_t tail;                                                           /* the position in the buffer where we start reading */
  uint32_t capacity;                                                       /* the allocated bytes of `buffer` */
  uint32_t nbytes;                                                         /* how many bytes have been written into the buffer */
  int is_init;                                                             /* 1 = yes, -1 = no */
};

int rxp_ringbuffer_init(rxp_ringbuffer* rb);                                /* initializes the struct; sets all members to defaults. when you initialized and/or allocated data, make sure to call rxp_ringbuffer_clear first or you'll be leaking memory. */
int rxp_ringbuffer_allocate(rxp_ringbuffer* rb, uint32_t nbytes);           /* allocate the internal buffer with nbytes of bytes*/
int rxp_ringbuffer_clear(rxp_ringbuffer* rb);                               /* frees allocated memory + calls rxp_ringbuffer_reset() */
int rxp_ringbuffer_write(rxp_ringbuffer* rb, void* data, uint32_t nbytes);  /* write some data into the buffer */
int rxp_ringbuffer_read(rxp_ringbuffer* rb, void* data, uint32_t nbytes);   /* read some data. returns < 0 when there is not enough data in the buffer. */
int rxp_ringbuffer_reset(rxp_ringbuffer* rb);                               /* set all members to default values */


#endif
