#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))
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

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

struct virtio_net_config {
  uint8 mac[6];
  uint16 status;
  uint16 max_virtqueue_pairs;
  uint16 mtu;
};


struct transmitq {
  // The descriptor table tells the device where to read and write
  // individual disk operations.
  struct virtq_desc *desc;
  // The available ring is where the driver writes descriptor numbers
  // that the driver would like the device to process (just the head
  // of each chain). The ring has NUM elements.
  struct virtq_avail *avail;
  // The used ring is where the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // The ring has NUM elements.
  struct virtq_used *used;

  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used->ring.
  // // disk command headers.
  // // one-for-one with descriptors, for convenience.
  // struct virtio_blk_req ops[NUM];
  
  struct spinlock vtransmitq_lock;
} transmitq;

struct receiveq {
  // The descriptor table tells the device where to read and write
  // individual disk operations.
  struct virtq_desc *desc;
  // The available ring is where the driver writes descriptor numbers
  // that the driver would like the device to process (just the head
  // of each chain). The ring has NUM elements.
  struct virtq_avail *avail;
  // The used ring is where the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // The ring has NUM elements.
  struct virtq_used *used;

  // // our own book-keeping.
  char free[NUM];  // is a descriptor free?
  uint16 used_idx; // we've looked this far in used->ring.

  // // track info about in-flight operations,
  // // for use when completion interrupt arrives.
  // // indexed by first descriptor index of chain.
  // struct {
  //   struct buf *b;
  //   char status;
  // } info[NUM];

  // // disk command headers.
  // // one-for-one with descriptors, for convenience.
  // struct virtio_blk_req ops[NUM];
  
  struct spinlock vreceiveq_lock;
} receiveq;



struct virtio_net_hdr {
  uint8 flags;
  uint8 gso_type;
  uint16 hdr_len;
  uint16 gso_size;
  uint16 csum_start;
  uint16 csum_offset;
  uint16 num_buffers;
};

// find a free descriptor, mark it non-free, return its index.
// static int
// alloc_desc(void)
// {
//   for(int i = 0; i < NUM; i++){
//     if(disk.free[i]){
//       disk.free[i] = 0;
//       return i;
//     }
//   }
//   return -1;
// }

// // a single descriptor, from the spec.
// struct virtq_desc {
//   uint64 addr;
//   uint32 len;
//   uint16 flags;
//   uint16 next;
// };

/* initialize the NIC and store the MAC address */
void virtio_net_init(void *mac) {
  uint32 status = 0;
  struct virtio_net_config * config;
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
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;


  // Tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // Ensure the FEATURES_OK bit is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio network FEATURES_OK unset");

  // read device configuration space
  config = (struct virtio_net_config *)R(VIRTIO_MMIO_CONFIG);

    // Initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net ready not zero");
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio net has no queue 0");
  if(max < NUM)
    panic("virtio net max queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)   = (uint64)receiveq.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)receiveq.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)receiveq.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)receiveq.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)receiveq.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)receiveq.used >> 32;

  for(int i = 0; i < NUM; i++)
    receiveq.free[i] = 1;
    // Initialize queue 1.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 1;
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio net ready not zero");
  max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio net has no queue 1");
  if(max < NUM)
    panic("virtio net max queue too short");
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)   = (uint64)transmitq.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)transmitq.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)transmitq.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)transmitq.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)transmitq.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)transmitq.used >> 32;
  /* Queue ready. */
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for(int i = 0; i < NUM; i++)
    transmitq.free[i] = 1;
  // Tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
// 52:54:00:12:34:56

  // mac = config->mac;
  // mac = (uint8 **)mac;
  for(int i = 0; i<6; i++){
    // printf("here we have %x\n", mac[i]);
    ((uint8 *)mac)[i] = config->mac[i];
    // (uint8 *) mac += 1;
  }

}

static int
alloc_desc_transmit(void)
{
  for(int i = 0; i < NUM; i++){
    if(transmitq.free[i]){
      transmitq.free[i] = 0;
      return i;
    }
  }
  return -1;
}

static int
alloc_desc_receive(void)
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
free_desc_transmit(int i)
{
  if(i >= NUM)
    panic("virtio_disk_intr 1");
  if(transmitq.free[i])
    panic("virtio_disk_intr 2");
  transmitq.desc[i].addr = 0;
  transmitq.free[i] = 1;
  // wakeup(&transmitq.free[0]);
}

// mark a descriptor as free.
static void
free_desc_receive(int i)
{
  if(i >= NUM)
    panic("virtio_disk_intr 1");
  if(receiveq.free[i])
    panic("virtio_disk_intr 2");
  receiveq.desc[i].addr = 0;
  receiveq.free[i] = 1;
  // wakeup(&receiveq.free[0]);
}

/* send data; return 0 on success */
int virtio_net_send(const void *data, int len) {
  return 0; // FIXME
}

/* receive data; return the number of bytes received */
int virtio_net_recv(void *data, int len) {
  return 0; // FIXME
}
