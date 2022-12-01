#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "proc.h"
#include "virtio.h"

#define DEBUGP 0
// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))
// this many virtio descriptors.
// must be a power of two.
#define NUM 8
// flags
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1
#define VIRTIO_NET_HDR_F_DATA_VALID 2
#define VIRTIO_NET_HDR_F_RSC_INFO 4

// headers
#define VIRTIO_NET_HDR_GSO_NONE 0
#define VIRTIO_NET_HDR_GSO_TCPV4 1
#define VIRTIO_NET_HDR_GSO_UDP 3
#define VIRTIO_NET_HDR_GSO_TCPV6 4
#define VIRTIO_NET_HDR_GSO_ECN 0x80

struct virtio_net_config {
  uint8 mac[6];
  uint16 status;
  uint16 max_virtqueue_pairs;
  uint16 mtu;
};


struct transmitq {
  // The descriptor table tells the device where to read and write
  // individual net operations.
  struct virtq_desc *desc;
  // The available ring is where the driver writes descriptor numbers
  // that the driver would like the device to process (just the head
  // of each chain). The ring has NUM elements.
  struct virtq_avail *avail;
  // The used ring is where the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // The ring has NUM elements.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used->ring.

  // net command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_net_hdr ops[NUM];
  
  struct spinlock vtransmitq_lock;
} transmitq;

struct receiveq {
  // The descriptor table tells the device where to read and write
  // individual net operations.
  struct virtq_desc *desc;
  // The available ring is where the driver writes descriptor numbers
  // that the driver would like the device to process (just the head
  // of each chain). The ring has NUM elements.
  struct virtq_avail *avail;
  // The used ring is where the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // The ring has NUM elements.
  struct virtq_used *used;

  // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used->ring.

  // // track info about in-flight operations,
  // // for use when completion interrupt arrives.
  // // indexed by first descriptor index of chain.
  // struct {
  //   struct buf *b;
  //   char status;
  // } info[NUM];

  // net command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio_net_hdr ops[NUM];
  
  struct spinlock vreceiveq_lock;
} receiveq;




// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc_receiveq(void)
{
  for(int i = 0; i < NUM; i++){
    if(receiveq.free[i]){
      receiveq.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc_receiveq(int i)
{
  if(i >= NUM)
    panic("virtio_net_recevie 1");
  if(receiveq.free[i])
    panic("virtio_net_recevie 2");
  receiveq.desc[i].addr = 0;
  receiveq.free[i] = 1;
  // wakeup(&receiveq.free[0]);
}

// static int
// alloc2_desc_receiveq(int *idx)
// {
//   for(int i = 0; i < 2; i++){
//     idx[i] = alloc_desc_receiveq();
//     if(idx[i] < 0){
//       for(int j = 0; j < i; j++)
//         free_desc_receiveq(idx[j]);
//       return -1;
//     }
//   }
//   return 0;
// }


// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc_transmitq(void)
{
  for(int i = 0; i < NUM; i++){
    if(transmitq.free[i]){
      transmitq.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc_transmitq(int i)
{
  if(i >= NUM)
    panic("virtio_disk_intr 1");
  if(transmitq.free[i])
    panic("virtio_disk_intr 2");
  transmitq.desc[i].addr = 0;
  transmitq.free[i] = 1;
  wakeup(&transmitq.free[0]);
}


// free a chain of descriptors.
static void
free_chain_transmitq(int i)
{
  while(1){
    free_desc_transmitq(i);
    if(transmitq.desc[i].flags & VIRTQ_DESC_F_NEXT)
      i = transmitq.desc[i].next;
    else
      break;
  }
}

// free a chain of descriptors.
static void
free_chain_receiveq(int i)
{
  while(1){
    free_desc_receiveq(i);
    if(receiveq.desc[i].flags & VIRTQ_DESC_F_NEXT)
      i = receiveq.desc[i].next;
    else
      break;
  }
}


static int
alloc2_desc_transmitq(int *idx)
{
  for(int i = 0; i < 2; i++){
    idx[i] = alloc_desc_transmitq();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc_transmitq(idx[j]);
      return -1;
    }
  }
  return 0;
}


/* initialize the NIC and store the MAC address */
void virtio_net_init(void *mac) {
  uint32 status = 0;

  // Initialize the transmit virtqueue
  initlock(&transmitq.vtransmitq_lock, "virtio_net_transmit");
 
  transmitq.desc = kalloc();
  transmitq.avail = kalloc();
  transmitq.used = kalloc();
  if(!transmitq.desc || !transmitq.avail || !transmitq.used)
    panic("virtio net transmitq kalloc");
  memset(transmitq.desc, 0, PGSIZE);
  memset(transmitq.avail, 0, PGSIZE);
  memset(transmitq.used, 0, PGSIZE);

  // Initialize the receive virtqueue
  initlock(&receiveq.vreceiveq_lock, "virtio_net_receive");

  receiveq.desc = kalloc();
  receiveq.avail = kalloc();
  receiveq.used = kalloc();
  if(!receiveq.desc || !receiveq.avail || !receiveq.used)
    panic("virtio net receiveq kalloc");
  memset(receiveq.desc, 0, PGSIZE);
  memset(receiveq.avail, 0, PGSIZE);
  memset(receiveq.used, 0, PGSIZE);

  // Check the device ID and MMIO version information
  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 1 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio net");
  }

  // Reset the device.
  *R(VIRTIO_MMIO_STATUS) = status;

  // Set the ACKNOWLEDGE bit.
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // Set the DRIVER bit.
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // Negotiate features.
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_NET_F_MAC);
  // features &= ~(1 << 15);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;


  // Tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // Ensure the FEATURES_OK bit is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio network FEATURES_OK unset");

  // Initialize queue 0: the receive virtqueue.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net receive queue ready not zero");
  uint32 max0 = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max0 == 0)
    panic("virtio net has no queue 0");
  if(max0 < NUM)
    panic("virtio net max receive queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)   = (uint64)receiveq.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)receiveq.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)receiveq.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)receiveq.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)receiveq.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)receiveq.used >> 32;

  /* Queue ready. */
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++){
    receiveq.free[i] = 1;
    // if (DEBUGP) printf("receiveq.free[%d] = %d\n", i, receiveq.free[i]);
  }
  
  
  // Initialize queue 1: the transmit virtqueue.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 1;
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net transmit queue ready not zero");
  uint32 max1 = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max1 == 0)
    panic("virtio net has no queue 0");
  if(max1 < NUM)
    panic("virtio net max transmit queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)   = (uint64)transmitq.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)transmitq.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)transmitq.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)transmitq.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)transmitq.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)transmitq.used >> 32;
  /* Queue ready. */
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    transmitq.free[i] = 1;


  // read data from the device configuration space
  struct virtio_net_config * config = (struct virtio_net_config *)R(VIRTIO_MMIO_CONFIG);

  // Set the MAC address from the device configuration space which is 52:54:00:12:34:56
  for(int i = 0; i < 6; i++){
    ((uint8 *)mac)[i] = config->mac[i];
  }

  // Tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

}

/* send data; return 0 on success */
// The driver adds outgoing (device readable) packets to the transmit virtqueue, and then frees them after they are used.
int virtio_net_send(const void *data, int len) {
  acquire(&transmitq.vtransmitq_lock);
  int idx[2];
  while(1){
    if (alloc2_desc_transmitq(idx) == 0){
      break;
    }
    sleep(&transmitq.free[0], &transmitq.vtransmitq_lock);
  } 

  // Format the two desciptors.
  struct virtio_net_hdr * hdr = &transmitq.ops[idx[0]];
  hdr->flags = 0;
  hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->num_buffers = 0; // unused field on transmitted packets
  hdr->hdr_len = sizeof(struct virtio_net_hdr);
  hdr->gso_size = 0;
  hdr->csum_start = 0;
  hdr->csum_offset = 0;

  transmitq.desc[idx[0]].addr = (uint64) hdr;
  transmitq.desc[idx[0]].len = sizeof(struct virtio_net_hdr);
  transmitq.desc[idx[0]].flags = VIRTQ_DESC_F_NEXT;
  transmitq.desc[idx[0]].next = idx[1];

  transmitq.desc[idx[1]].addr = (uint64) data;
  transmitq.desc[idx[1]].len = len;
  transmitq.desc[idx[1]].flags = 0;
  transmitq.desc[idx[1]].next = 0;

  // avail->idx tells the device how far to look in avail->ring.
  // avail->ring[...] are desc[] indices the device should process.
  // we only tell device the first index in our chain of descriptors.
  transmitq.avail->ring[transmitq.avail->idx % NUM] = idx[0];
  __sync_synchronize();
  transmitq.avail->idx += 1;

  if (DEBUGP) printf("send before 1: %d\n", transmitq.used->idx);
  if (DEBUGP) printf("send before 2: %d\n", transmitq.used_idx);
  *R(VIRTIO_MMIO_QUEUE_SEL) = 1;
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 1; // value is queue number
  if (DEBUGP) printf("sent len: %d\n", len);
  while(transmitq.used_idx >= transmitq.used->idx){
    // wait for queue to add something to used queue
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 1;
  }
  transmitq.used_idx = transmitq.used->idx;
  free_chain_transmitq(idx[0]);

  release(&transmitq.vtransmitq_lock);
  return 0;
}

/* receive data; return the number of bytes received */
// Incoming (device writable) buffers are added to the receive virtqueue, and processed after they are used.
int virtio_net_recv(void *data, int len) {
  acquire(&receiveq.vreceiveq_lock);
  int idx;
  idx = alloc_desc_receiveq();

  // Format the two desciptors.
  struct virtio_net_hdr * hdr = &receiveq.ops[idx];

  hdr->flags = 0;
  hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
  hdr->num_buffers = 1;
  hdr->hdr_len = sizeof(struct virtio_net_hdr);
  hdr->gso_size = 0;
  hdr->csum_start = 0;
  hdr->csum_offset = 0;
  *((struct virtio_net_hdr *) data) = *hdr;
  receiveq.desc[idx].addr = (uint64) data;
  receiveq.desc[idx].len = len;
  receiveq.desc[idx].flags = VIRTQ_DESC_F_WRITE;
  receiveq.desc[idx].next = 0;

  // avail->idx tells the device how far to look in avail->ring.
  // avail->ring[...] are desc[] indices the device should process.
  // we only tell device the first index in our chain of descriptors.
  receiveq.avail->ring[receiveq.avail->idx % NUM] = idx;
  receiveq.avail->idx += 1;
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number
  int counter = 0;
  while(receiveq.used_idx == receiveq.used->idx){
    // wait for queue to add something to used queue
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number
    counter++;
    if(counter > 1000){
      break;
    }
  }
  int actual_len = 0;

 
  actual_len += receiveq.used->ring[receiveq.used_idx % NUM].len;
  free_chain_receiveq(receiveq.used->ring[receiveq.used_idx % NUM].id);
  
  char* holder = (char *)data + 12;
  for(int i = 12; i<actual_len; i++){
    *((char *)data) = *holder;
    holder+=1;
    (char *)data++;
  }
  receiveq.used_idx = receiveq.used->idx;

  release(&receiveq.vreceiveq_lock);
  return actual_len-12;
}
