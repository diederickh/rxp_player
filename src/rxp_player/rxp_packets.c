#include <stdio.h>
#include <stdlib.h>
#include <rxp_player/rxp_packets.h>
#include <rxp_player/rxp_types.h>

int rxp_img_init(rxp_img* img) {
  if (!img) { return -1; } 
  img->width = 0;
  img->height = 0;
  img->stride = 0;
  img->data = NULL;
  return 0;
}

int rxp_packet_dealloc(rxp_packet* pkt) {

  if (!pkt) { return -1; } 

  /* not really necessary; but nice to reset mem */
  pkt->prev = NULL;
  pkt->next = NULL;
  pkt->type = RXP_NONE;
  pkt->pts = 0;
  pkt->size = 0;
  pkt->is_free = 0;

  rxp_img_init(&pkt->img[0]);
  rxp_img_init(&pkt->img[1]);
  rxp_img_init(&pkt->img[2]);

  if (pkt->data) {
    free(pkt->data);
    pkt->data = NULL;
  }

  free(pkt);
  pkt = NULL;

  return 0;
}

rxp_packet* rxp_packet_alloc() {

  rxp_packet* pkt = (rxp_packet*)malloc(sizeof(rxp_packet));
  if (!pkt) {
    printf("Error: cannot allocate a rxp_packet.\n");
    return NULL;
  }
  
  pkt->prev = NULL;
  pkt->next = NULL;
  pkt->data = NULL;
  pkt->type = RXP_NONE;
  pkt->pts = 0;
  pkt->size = 0;
  pkt->is_free = 1;

  rxp_img_init(&pkt->img[0]);
  rxp_img_init(&pkt->img[1]);
  rxp_img_init(&pkt->img[2]);

  return pkt;
}

rxp_packet_queue* rxp_packet_queue_alloc() {
  
  rxp_packet_queue* q = (rxp_packet_queue*)malloc(sizeof(rxp_packet_queue));
  if (!q) {
    printf("Error: cannot allocate a rxp_packet_queue.\n");
    return NULL;
  }
  
  rxp_packet_queue_init(q);
  return q;
}

int rxp_packet_queue_dealloc(rxp_packet_queue* q) {

  rxp_packet* tail = NULL;
  rxp_packet* next = NULL;

  if (!q) { return -1; } 

  if (q->is_init == -1) { 
    printf("Info: you're trying to deallocate a packet queue which is already deallocated.\n");
    return 0;
  }

  tail = q->packets;
  
  rxp_packet_queue_lock(q);
  {
    while (tail) {
      next = tail->next;
      rxp_packet_dealloc(tail);
      tail = next;
    }
  }
  rxp_packet_queue_unlock(q);

  q->packets = NULL;
  q->last_packet = NULL;
  q->is_init = -1;

  uv_mutex_destroy(&q->mutex);
  
  return 0;
}

int rxp_packet_queue_init(rxp_packet_queue* q) {

  if (!q) { return -1; } 

  q->packets = NULL;
  q->last_packet = NULL;
  q->is_init = 1;

  uv_mutex_init(&q->mutex);

  return 0;
}

/* append a new packet to the queue, while locking the mutex */
int rxp_packet_queue_add(rxp_packet_queue* q, rxp_packet* pkt) {
  
  if (!q) { return -1; } 
  if (!pkt) { return -2; } 

  uv_mutex_lock(&q->mutex);
  {
    pkt->is_free = 0;
    if (!q->last_packet) {
      q->packets = pkt;
    }
    else {
      pkt->prev = q->last_packet;
      q->last_packet->next = pkt;
    }
    q->last_packet = pkt;
  }
  uv_mutex_unlock(&q->mutex);

  return 0;
}

int rxp_packet_queue_remove(rxp_packet_queue* q, rxp_packet* pkt) {
  
  if (!q) { return -1; } 
  if (!pkt) { return -2; } 


  if (pkt->prev && pkt->next) { 
    /* between two packets */
    pkt->prev->next = pkt->next;
    pkt->next->prev = pkt->prev;
  }
  else if (!pkt->next && pkt->prev) {
    /* end of list */
    pkt->prev->next = NULL;
  }
  else if (!pkt->prev) { 

    if (pkt->next) {
      /* beginning of list */
      pkt->next->prev = NULL;
      q->packets = pkt->next;
    }
    else {
      /* only packet available */
      q->last_packet = NULL;
      q->packets = NULL;
    }
  }

  rxp_packet_dealloc(pkt);
  pkt = NULL;
  return 0;
}

rxp_packet* rxp_packet_queue_find_free_packet(rxp_packet_queue* q) {
  
  rxp_packet* pkt = NULL;

  if (!q) { return NULL; }

  pkt = q->packets;

  while (pkt) {
    if (pkt->is_free == 1) {
      return pkt;
    }
    pkt = pkt->next;
  }

  return NULL;
}

void rxp_packet_queue_lock(rxp_packet_queue* q) {
  if (!q) { return ; }
  uv_mutex_lock(&q->mutex);
}

void rxp_packet_queue_unlock(rxp_packet_queue* q) {
  if (!q) { return ; }
  uv_mutex_unlock(&q->mutex);
}

