/*

  rxp_packets
  -----------
  
  rxp_packets is used to store and retrieve video packets that the 
  decoder produces. See rxp_player.c where we use this struct. 


 */

#ifndef RXP_PACKETS_H
#define RXP_PACKETS_H

#include <stdint.h>
#include <uv.h>

typedef struct rxp_img rxp_img;
typedef struct rxp_packet rxp_packet;
typedef struct rxp_packet_queue rxp_packet_queue;

/* Image plane */
struct rxp_img {
  uint16_t width;
  uint16_t height;
  uint16_t stride;
  uint8_t* data;
};

/* Packet used to store video frames */
struct rxp_packet {
  uint8_t* data;
  int size;
  int type;
  int is_free;
  uint64_t pts;
  rxp_img img[3];                                                        /* is used for video frames */
  rxp_packet* next;
  rxp_packet* prev;
};

/* The queue */
struct rxp_packet_queue {
  rxp_packet* packets;
  rxp_packet* last_packet;
  uv_mutex_t mutex;
  int is_init;                                                           /* is set to 1 when initialized, otherwise -1 */ 
};

/* Image */
int rxp_img_init(rxp_img* img);

/* Packets */
rxp_packet* rxp_packet_alloc();                                          /* allocate a new rxp_packet that will contain audio/video/... data  */
int rxp_packet_dealloc(rxp_packet* pkt);                                 /* deallocate a packet */

/* Packet queue */
rxp_packet_queue* rxp_packet_queue_alloc();                              /* allocate a packet queue */
int rxp_packet_queue_dealloc(rxp_packet_queue* q);                       /* deallocates all the packets in the queue, it will lock the queue. when the queue is already deallocated this will return 0 */
int rxp_packet_queue_init(rxp_packet_queue* queue);                      /* initialize an allocated packet queue */
int rxp_packet_queue_add(rxp_packet_queue* q, rxp_packet* pkt);          /* append a packet to the queue */
int rxp_packet_queue_remove(rxp_packet_queue* q, rxp_packet* pkt);       /* remove a packet from the queue */
rxp_packet* rxp_packet_queue_find_free_packet(rxp_packet_queue* q);      /* find a free packet in the queue */
void rxp_packet_queue_lock(rxp_packet_queue* q);                         /* lock when you want to iterate over the list */
void rxp_packet_queue_unlock(rxp_packet_queue* q);                       /* unlock when you're ready itetration */

#endif
