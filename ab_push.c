#include <stdio.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#define ABLETON_VENDOR_ID 0x2982
#define PUSH2_PRODUCT_ID  0x1967

#define PUSH2_BULK_EP_OUT 0x01
#define TRANSFER_TIMEOUT 1000 // ms
#define LINE_SIZE 2048
#define LINES_PER_BUFFER 1
#define BUFFER_COUNT 1
#define BUFFER_SIZE LINES_PER_BUFFER * LINE_SIZE

static unsigned char frame_header[16] = {
    0xff, 0xcc, 0xaa, 0x88,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static libusb_device_handle* open_push2_device();
static void close_push2_device(libusb_device_handle* device_handle);

void test() {

    unsigned char frame_buffer[BUFFER_COUNT * BUFFER_SIZE];

    for (size_t i = 0; i < BUFFER_SIZE; i++) {
	frame_buffer[i] = i;
    }

    libusb_device_handle* push2 = open_push2_device();
    int actual_length;
    int r = libusb_bulk_transfer(push2, PUSH2_BULK_EP_OUT, frame_header, sizeof(frame_header), &actual_length, 0);

    if (r != 0) {
	fprintf(stderr, "transfer failed with %d", r);
	exit(EXIT_FAILURE);
    } else if (actual_length != sizeof(frame_header)) {
	fprintf(stderr, "%d of %ld bytes transfered\n", actual_length, sizeof(frame_header));
	exit(EXIT_FAILURE);
    }

    // sending LINES_PER_BUFFER lines
    for (int i = 0; i < 160 / LINES_PER_BUFFER; i++) {
	unsigned char* buffer = frame_buffer;
	r = libusb_bulk_transfer(push2, PUSH2_BULK_EP_OUT, buffer, BUFFER_SIZE, &actual_length, 0);

	if (r != 0) {
	    fprintf(stderr, "transfer failed with %d", r);
	    exit(EXIT_FAILURE);
	} else if (actual_length != BUFFER_SIZE) {
	    fprintf(stderr, "%d of %d bytes transfered\n", actual_length, BUFFER_SIZE);
	    exit(EXIT_FAILURE);
	}
    }

    close_push2_device(push2);
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
