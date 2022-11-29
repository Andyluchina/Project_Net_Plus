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

// // free a chain of descriptors.
// static void
// free_chain_receiveq(int i)
// {
//   while(1){
//     free_desc_receiveq(i);
//     if(receiveq.desc[i].flags & VIRTQ_DESC_F_NEXT)
//       i = receiveq.desc[i].next;
//     else
//       break;
//   }
// }
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
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  // printf("The address of lock is %p.\n", &receiveq.vreceiveq_lock);
  // printf("01 The locked value is %d.\n", receiveq.vreceiveq_lock.locked);
  // printf("The lock name is %s.\n", receiveq.vreceiveq_lock.name);
  acquire(&receiveq.vreceiveq_lock);
  // printf("02 The locked value is %d.\n", receiveq.vreceiveq_lock.locked);
  int idx;
  idx = alloc_desc_receiveq();
  printf("Yes got the descriptor %d\n", idx);
  // struct proc *p = myproc();
  // printf("process info: PID = %d\n", p);
  // while(1){    
  //   // printf("receiveq.free[0] = %d\n", receiveq.free[0]);
  //   // printf("&receiveq.free[0] = %p\n", &receiveq.free[0]);
  //   if ((idx = alloc_desc_receiveq()) == 0){
  //     printf("Yes got the descriptors!\n");
  //     break;
  //   }
  //   // printf("\n");
  //   // printf("\n");
  //   // printf("The locked value is %d.\n", receiveq.vreceiveq_lock.locked);
  //   // printf("receiveq.free[0] = %d\n", receiveq.free[0]);
  //   // printf("&receiveq.free[0] = %p\n", &receiveq.free[0]);
  //   sleep(&receiveq.free[0], &receiveq.vreceiveq_lock);
  // } 

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

  // receiveq.desc[idx[1]].addr = (uint64) data;
  // receiveq.desc[idx[1]].len = len;
  // receiveq.desc[idx[1]].flags = VIRTQ_DESC_F_WRITE;
  // receiveq.desc[idx[1]].next = 0;

  // avail->idx tells the device how far to look in avail->ring.
  // avail->ring[...] are desc[] indices the device should process.
  // we only tell device the first index in our chain of descriptors.
  receiveq.avail->ring[receiveq.avail->idx % NUM] = idx;
  receiveq.avail->idx += 1;
  __sync_synchronize();

  printf("before 1: %d\n", receiveq.used->idx);
  printf("before 2: %d\n", receiveq.used_idx);
  // uint16 idx_of_used;
  // idx_of_used = receiveq.used->idx;
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number
  
  while(receiveq.used_idx >= receiveq.used->idx){
    // wait for queue to add something to used queue
    // printf("waiting !!!!\n");
  }
  printf("after1: %d\n", receiveq.used->idx);
  printf("after2: %d\n", receiveq.used_idx);
  // receiveq.avail->idx -= 1;
  uint32 desc_id_used = receiveq.used->ring[receiveq.used_idx].id;
  // uint32 desc_id_used = receiveq.used->ring[receiveq.used->idx].id;
  // uint32 desc_id_used = receiveq.used->ring[2].id;
  printf("desc used is %d\n", desc_id_used);
  // struct virtq_desc* desc_head = &receiveq.desc[desc_id_used];
  int actual_len = 0;
  // do{
  //   // desc_head.addr;
  //  actual_len += receiveq.desc[desc_id_used].len;
  //  desc_id_used = receiveq.desc[desc_id_used].next;
  //  printf("added len %d\n", actual_len);
  //   // memset(transmitq.desc, 0, PGSIZE);
  // }while(desc_id_used !=0);

  // actual_len += receiveq.desc[desc_id_used].len;
  actual_len += receiveq.used->ring[receiveq.used_idx].len;
  // desc_id_used = receiveq.desc[desc_id_used].next;
  printf("added len %d\n", actual_len);
  free_desc_receiveq(receiveq.used->ring[receiveq.used_idx].id);
  receiveq.used_idx = receiveq.used->idx;
  // receiveq.used_idx ++;
  char* holder = (char *)data + 12;
  for(int i = 12; i<actual_len; i++){
    *((char *)data) = *holder;
    holder+=1;
    (char *)data++;
  }
  // uint16 r = 65535;
  // printf("%d before add 1\n", r);
  // printf("%d after add 1\n", r+1);
  
//   struct virtq_used_elem {
//   uint32 id;   // index of start of completed descriptor chain
//   uint32 len;
// };

// struct virtq_used {
//   uint16 flags; // always zero
//   uint16 idx;   // device increments when it adds a ring[] entry
//   struct virtq_used_elem ring[];
// };
// a single descriptor, from the spec.
// struct virtq_desc {
//   uint64 addr;
//   uint32 len;
//   uint16 flags;
//   uint16 next;
// };


  release(&receiveq.vreceiveq_lock);
  printf("%d\n", actual_len);
  return actual_len-12;
}
