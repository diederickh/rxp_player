#include <string.h>
#include <rxp_player/rxp_ringbuffer.h>

/* initialize a ringbuffer - sets default members, if you allocated something make sure to call clear() first. . */
int rxp_ringbuffer_init(rxp_ringbuffer* rb) {

  if (!rb) { return -1; } 
  
  rb->buffer = NULL;
  rb->head = 0;
  rb->tail = 0;
  rb->capacity = 0;
  rb->nbytes = 0;
  rb->is_init = 1;
  
  return 0;
}

/* allocate a new buffer for nbytes */
int rxp_ringbuffer_allocate(rxp_ringbuffer* rb, uint32_t nbytes) {

  if (!rb) { return -1; } 
  if (!nbytes) { return -2; } 

  if (rb->is_init == 1) {
    printf("Info: the ringbuffer is already initialized. ignoring request.\n");
    return 0;
  }

  rb->buffer = (uint8_t*)malloc(nbytes);
  if (!rb->buffer) { 
    printf("Error: cannot allocated the ring buffer.\n");
    return -3;
  }
  
  memset((void*)rb->buffer, 0x00, nbytes);

  rb->head = 0;
  rb->tail = 0;
  rb->capacity = nbytes;
  rb->nbytes = 0;


  return 0;
}

/* write nbytes of data */
int rxp_ringbuffer_write(rxp_ringbuffer* rb, void* data, uint32_t nbytes) {

  uint32_t space;

  if (!rb) { return -1; } 
  if (!data) { return -2; } 
  if (!nbytes) { return -3; } 

  if (nbytes > rb->capacity) {
    printf("Error: ringbuffer is not big enough to hold this amount of data.\n");
    return -4;
  }

  /* do we have enough space to write the complete block */
  space = rb->capacity - rb->head;

  if (space >= nbytes) {
    memcpy(rb->buffer + rb->head, data, nbytes);
    rb->head += nbytes;
    rb->nbytes += nbytes;
  }
  else {

    /* copy as much as possible to the end */
    memcpy(rb->buffer + rb->head, data, space);
    
    /* copy the remaining bytes to the start */
    rb->head = (nbytes - space);
    memcpy(rb->buffer, data, rb->head);
    rb->nbytes += nbytes;
  }
  return 0;
}

int rxp_ringbuffer_read(rxp_ringbuffer* rb, void* data, uint32_t nbytes) {

  uint32_t end, read_from_start, read_to_end;

  if (!rb) { return -1; } 
  if (!data) { return -2; } 
  if (!nbytes) { return -3; } 
  if (!rb->nbytes) { return -4; } 
  
  if (nbytes > rb->nbytes) {
    nbytes = rb->nbytes;
  }

  end = (rb->tail + nbytes);
  if (end < rb->capacity) {
    memcpy(data, rb->buffer + rb->tail, nbytes);
    rb->tail = (rb->tail + nbytes) % rb->capacity;
    rb->nbytes -= nbytes;
  }
  else {
    read_from_start = end - rb->capacity;
    read_to_end = nbytes - read_from_start;
    memcpy(data, rb->buffer + rb->tail, read_to_end);
    memcpy((void*)((uint8_t*)data + read_to_end), rb->buffer, read_from_start);
    rb->tail = read_from_start;
    rb->nbytes -= nbytes;
  }
  return 0;
}

int rxp_ringbuffer_reset(rxp_ringbuffer* rb) {
  if (!rb) { return -1; } 
  rb->head = 0;
  rb->tail = 0;
  rb->nbytes = 0;
  return 0;
}

int rxp_ringbuffer_clear(rxp_ringbuffer* rb) {
  if (!rb) { return -1; } 
  if (!rb->buffer) { return -2; } 
  
  if (rb->is_init != 1) {
    printf("Info: ringbuffer not initialized so we're not clearing it. ignoring request.\n");
    return 0;
  }
  printf("free! %d\n", rb->is_init);
  free(rb->buffer);
  rb->buffer = NULL;
  rb->capacity = 0;
  rb->is_init = -1;

  return rxp_ringbuffer_reset(rb);
}
