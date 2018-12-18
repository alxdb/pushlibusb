#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#define ABLETON_VENDOR_ID 0x2982
#define PUSH2_PRODUCT_ID  0x1967

#define PUSH2_BULK_EP_OUT 0x01
#define TRANSFER_TIMEOUT 1000 // ms
#define LINE_SIZE 2048
#define LINES_PER_BUFFER 8
#define BUFFER_COUNT 3
#define BUFFER_SIZE LINES_PER_BUFFER * LINE_SIZE

static unsigned char frame_header[16] = {
  0xff, 0xcc, 0xaa, 0x88,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

static libusb_device_handle* open_push2_device();
static void close_push2_device(libusb_device_handle* device_handle);
static void transfer_callback(struct libusb_transfer* transfer);
static struct libusb_transfer* alloc_and_fill_transfer(libusb_device_handle* device,
						       unsigned char* data,
						       size_t data_size);
static void start_transfer(struct libusb_transfer* transfer);

void test() {

  unsigned char frame_buffer[BUFFER_COUNT * BUFFER_SIZE];

  for (size_t i = 0; i < BUFFER_SIZE; i++) {
    frame_buffer[i] = 0;
  }

  libusb_device_handle* push2 = open_push2_device();
  struct libusb_transfer* frame_header_transfer = alloc_and_fill_transfer(push2, frame_header, sizeof(frame_header));

  start_transfer(frame_header_transfer);

  // sending LINES_PER_BUFFER lines
  for (int i = 0; i < BUFFER_COUNT; i++) {
    unsigned char* buffer = (frame_buffer + i * BUFFER_SIZE);
    struct libusb_transfer* buffer_transfer = alloc_and_fill_transfer(push2, buffer, BUFFER_SIZE);
    start_transfer(buffer_transfer);
  }

  close_push2_device(push2);
}

static void start_transfer(struct libusb_transfer* transfer) {
  if (libusb_submit_transfer(transfer) < 0) {
    fprintf(stderr, "transfer failed to submit\n");
    exit(EXIT_FAILURE);
  }
}

static struct libusb_transfer* alloc_and_fill_transfer(libusb_device_handle* device,
						       unsigned char* data,
						       size_t data_size) {
  struct libusb_transfer* transfer;

  if ((transfer = libusb_alloc_transfer(0)) == NULL) {
    fprintf(stderr, "error: couldn't allocate transfer\n");
    exit(EXIT_FAILURE);
  }

  libusb_fill_bulk_transfer(transfer,
			    device,
			    PUSH2_BULK_EP_OUT,
			    data,
			    data_size,
			    transfer_callback,
			    NULL,
			    TRANSFER_TIMEOUT);

  return transfer;
}

static void transfer_callback(struct libusb_transfer* transfer) {
  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    switch (transfer->status) {
    case LIBUSB_TRANSFER_ERROR: fprintf(stderr, "transfer failed\n"); break;
    case LIBUSB_TRANSFER_TIMED_OUT: fprintf(stderr, "transfer timeout\n"); break;
    case LIBUSB_TRANSFER_CANCELLED: fprintf(stderr, "transfer cancelled\n"); break;
    case LIBUSB_TRANSFER_STALL: fprintf(stderr, "transfer stalled\n"); break;
    case LIBUSB_TRANSFER_NO_DEVICE: fprintf(stderr, "no device for transfer\n"); break;
    case LIBUSB_TRANSFER_OVERFLOW: fprintf(stderr, "transfer overflow\n"); break;
    default: fprintf(stderr, "transfer failed with %d\n", transfer->status); break;
    }
    exit(EXIT_FAILURE);
  } else if (transfer->length != transfer->actual_length) {
    fprintf(stderr, "only %d out of %d bytes transfered\n", transfer->length, transfer->actual_length);
  } else {
    puts("transfer success");
  }
}

static libusb_device_handle* open_push2_device() {
  int result;
  if ((result = libusb_init(NULL)) < 0) { // Initialize libusb
    fprintf(stderr, "error: [%d] could not initialize libusb\n", result);
    exit(EXIT_FAILURE);
  }
  libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_ERROR);

  libusb_device** devices;
  ssize_t count = libusb_get_device_list(NULL, &devices); // Populate device list
  if (count < 0) {
    fprintf(stderr, "error: [%ld] could not get usb device list\n", count);
    exit(EXIT_FAILURE);
  }

  libusb_device* device;
  libusb_device_handle* device_handle = NULL;

  for (ssize_t i = 0; i < count; i++) {
    device = devices[i];
    struct libusb_device_descriptor descriptor;
    if ((result = libusb_get_device_descriptor(device, &descriptor)) < 0) {
      printf("warning: [%d] could not get device descriptor for device %ld\n", result, i);
      continue;
    }

    if (descriptor.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE
	&& descriptor.idVendor  == ABLETON_VENDOR_ID
	&& descriptor.idProduct == PUSH2_PRODUCT_ID) {
      if ((result = libusb_open(device, &device_handle)) < 0) {
	fprintf(stderr, "error: [%d] could not open push2\n", result);
	exit(EXIT_FAILURE);
      }
      if ((result = libusb_claim_interface(device_handle, 0)) < 0) {
	fprintf(stderr, "error: [%d] could not claim interface 0 of push2\n", result);
	libusb_close(device_handle);
	exit(EXIT_FAILURE);
      }
      printf("opened push2 device at %p\n", device_handle);
      break; // Success
    }
  }

  if (device_handle == NULL) {
    fprintf(stderr, "error: push2 not found\n");
    exit(EXIT_FAILURE);
  }

  libusb_free_device_list(devices, 1);
  return device_handle;
}

static void close_push2_device(libusb_device_handle* device_handle) {
  libusb_release_interface(device_handle, 0);
  libusb_close(device_handle);
}
