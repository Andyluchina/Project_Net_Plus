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

struct virtio_net_config {
  uint8 mac[6];
  uint16 status;
  uint16 max_virtqueue_pairs;
  uint16 mtu;
};

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
  // printf("here we have %d\n", config -> mac);

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

/* send data; return 0 on success */
int virtio_net_send(const void *data, int len) {
  return 0; // FIXME
}

/* receive data; return the number of bytes received */
int virtio_net_recv(void *data, int len) {
  return 0; // FIXME
}
