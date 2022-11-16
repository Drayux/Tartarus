#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>      // This import might not be needed
#include <linux/hid.h>
#include <linux/string.h>   // For use with memset and memcpy
// #include <linux/usb/input.h>
// #include <linux/slab.h>
// #include <linux/dmi.h>


/*/ MISCELLANEOUS THINGS I MIGHT NEED

// usb_device struct reference
https://docs.kernel.org/driver-api/usb/usb.html#c.usb_device


// This is the struct that they put in the hid_device struct itself (at hid_device->dev ???)
// I *think* this is freeform because it takes a pointer
struct razer_kbd_device {
    unsigned int fn_on;
    DECLARE_BITMAP(pressed_fn, KEY_CNT);	// Pretty sure this reduces to an array of unsigned longs

    unsigned char block_keys[3];
    unsigned char left_alt_on;
};


// Macro for file creation -- Move this to compiled code
#define CREATE_DEVICE_FILE(dev, type) \
do { \
    if(device_create_file(dev, type)) { \
        goto exit_free; \
    } \
} while (0)


// LED definitions
#define ZERO_LED          0x00
// ...
#define RED_PROFILE_LED   0x0C
#define GREEN_PROFILE_LED 0x0D
#define BLUE_PROFILE_LED  0x0E
// ...


// Report Responses -- DEFINITELY NEED THESE
#define RAZER_CMD_BUSY          0x01
#define RAZER_CMD_SUCCESSFUL    0x02
#define RAZER_CMD_FAILURE       0x03
#define RAZER_CMD_TIMEOUT       0x04
#define RAZER_CMD_NOT_SUPPORTED 0x05


// This might just be used for FN-key handling (which I do not have)
struct razer_key_translation {
    u16 from;
    u16 to;
    u8 flags;
};

/*/

// Driver metadata
MODULE_AUTHOR("Drayux");
MODULE_DESCRIPTION("Tartarus V2 Driver");
MODULE_LICENSE("GPL");


// -- DEVICE SUPPORT --
#define VENDOR_ID	0x1532		// Razer USA, Ltd
#define PRODUCT_ID	0x022b		// Tartarus_V2

#define REPORT_LEN  0x5A        // Each USB report has 90 bytes
#define REPORT_IDX  0x01        // Report/Response (consistent across all devices so I don't think I need two defs)
#define WAIT_MIN    600         // Minmum response wait time is 600 microseconds (0.6 ms)
#define WAIT_MAX    800         // ^^Maximum is 800 us (0.8 ms)

// Define device commands
#define CMD_KBD_LAYOUT      0x00, 0x86, 0x02

static int tartarus_probe(struct hid_device*, const struct hid_device_id*);
static void tartarus_disconnect(struct hid_device*);
// static int event_handler(struct hid_device*, struct hid_field*, struct hid_usage*, int32_t);
// static int raw_event_handler(struct hid_device*, struct hid_report*, u8*, int);
static int tartarus_input_mapping(struct hid_device* dev, struct hid_input* input, struct hid_field* field,
                             struct hid_usage* usage, unsigned long** bit, int* max) { return 0; }

// Array of USB device ID structs
static struct hid_device_id id_table [] = {
	{ HID_USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ 0 }
}; MODULE_DEVICE_TABLE(hid, id_table);

// Driver metadata struct
static struct hid_driver tartarus_driver = {
    .name = "razer_tartarus_usb",
    .id_table = id_table,
    .probe = tartarus_probe,
    .remove = tartarus_disconnect,
    // .event = event_handler,
    // .raw_event = raw_event_handler,
    .input_mapping = tartarus_input_mapping
};

module_hid_driver(tartarus_driver);


// -- DATATYPES --
// Format of the 90 byte device response
// ^^(Taken directly from OpenRazer)
struct razer_report {
    // Response status
    //  0x00 - New Command
    //  0x01 - Device Busy
    //  0x02 - Success
    //  0x03 - Failure
    //  0x04 - Timeout
    //  0x05 - Not Supported
    unsigned char status;

    // Transaction ID (transaction_id)
    // Allows for communicating with multiple devices on one USB
    // (Still haven't figured out how the interface works yet--probably not needed)
    union _transaction_id {
        unsigned char id;
        struct transaction_parts {
            unsigned char device : 3;
            unsigned char id : 5;
        } parts;
    } tr_id;

    // Remaining packets (remaining_packets)
    // Big endian from device
    // (Also not sure yet how this is used/what exactly it means in this scope)
    unsigned short remaining;

    // Protocol Type (protocol_type)
    // Only provided comment is 'always 0x0'
    // Probably deprecated Razer feature
    unsigned char type;

    // Size of the payload (data_size)
    // Maximum is 80 bytes as the header and 'footer' components consume 10
    unsigned char size;

    // Command type (command_class)
    // Use this to specify what the device should do for us (basically our call parameter)
    // Device should always return with the same class as the request
    // ????   Actually this could be command_id - Needs more digging   ?????
    unsigned char class;

    // Command ID (command_id)
    // The exact command we are using
    // Direction [0] is   Host -> Device
    //           [1] is Device -> Host
    union _command_id {
        unsigned char id;
        struct command_id_parts {
            unsigned char direction : 1;
            unsigned char id : 7;
        } parts;
    } cmd_id;

    // Command data
    // If [Host -> Device] : This is where we place command arguments
    // If [Device -> Host] : This contains the device response data
    unsigned char data[80];

    // Argument/Data checksum (crc)
    // Algorithm XORs each byte of the report, starting at 2 and ending at 87 (inclusive) [Skips bytes 0, 1, 88, 89]
    unsigned char cksum;

    // Unused, always 0x0
    unsigned char reserved;
};

// Create a razer report data type
// Specifically for use in requesting data from the device
// (The device will populate this itself when it replies, so some fields need not be variable)
// Pulled almost directly from OpenRazer
struct razer_report generate_report(unsigned char class, unsigned char id, unsigned char size) {
    struct razer_report report = { 0 };
    // Static values
    report.tr_id.id = 0xFF;
    // report.status = 0x00;
    // report.remaining = 0x00;
    // report.protocol_type = 0x00;

    // Command parameters
    report.class = class;
    report.cmd_id.id = id;
    report.size = size;

    return report;
}

// Log the a razer report struct to the kernel (currently used for debugging)
// Also taken mostly from OpenRazer
void log_report(struct razer_report* report) {
    printk(KERN_INFO "status: %02x transaction_id.id: %02x remaining_packets: %02x protocol_type: %02x data_size: %02x, command_class: %02x, command_id.id: %02x Params: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x .\n",
           report->status,
           report->tr_id.id,
           report->remaining,
           report->type,
           report->size,
           report->class,
           report->cmd_id.id,
           report->data[0], report->data[1], report->data[2], report->data[3], report->data[4], report->data[5],
           report->data[6], report->data[7], report->data[8], report->data[9], report->data[10], report->data[11],
           report->data[12], report->data[13], report->data[14], report->data[15]);
}


// -- DEVICE INTERFACING (main driver goodness) --
// Calculate report checksum
// ^^Taken directly from OpenRazer driver
unsigned char razer_checksum(struct razer_report* report) {
    unsigned char ck = 0;
    unsigned char* bytes = (unsigned char*) report;

    for (int i = 2; i < 88; i++) ck ^= bytes[i];
    return ck;
}

// Monolithic function to communcate with device
// Returns the device response
// Use req (aka the request report) to specify the command to send
// cmd_errno can be NULL (but shouldn't be)
// Core functionality taken from OpenRazer
// POTENTIAL TODO: Change this to take a usb_device instead of an hid_device
static struct razer_report send_command(struct hid_device* dev, struct razer_report* req, int* cmd_errno) {
    char* request;      // razer_report containing the command parameters (i.e. get layout/set lighting pattern/etc)
    int received = -1;  // Amount of data transferred (result of usb_control_msg)

    // Cast our HID device to a USB device (I want to think there's a better option than this?)
    // Note to self--Still not sure where we specify the interface...maybe this selects just the one we want?
    struct usb_interface* interface = to_usb_interface(dev->dev.parent);
    struct usb_device* tartarus = interface_to_usbdev(interface);

    // Function output
    struct razer_report response = { 0 };

    // Allocate necessary memory and check for errors
    request = (char*) kzalloc(sizeof(struct razer_report), GFP_KERNEL);
    if (!request) {         // (!(request && response))
        printk(KERN_WARNING "Failed to communcate with device: Out of memory.\n");
        *cmd_errno = -ENOMEM;
        return response;
    }

    // Copy data from our request struct to the fresh pointer
    req->cksum = razer_checksum(req);
    memcpy(request, req, sizeof(struct razer_report));

    // Step one - Attempt to send the control report
    // This sends the data to the device and sets the internal "respond to me" bit
    //  0x09 --> HID_REQ_SET_REPORT
    //  0X21 --> USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT
    received = usb_control_msg(tartarus, usb_sndctrlpipe(tartarus, 0), 0x09, 0X21, 0x300, REPORT_IDX, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);
    usleep_range(WAIT_MIN, WAIT_MAX);

    // TODO CHECK FOR ERRORS! (received != <expected size>)
    if (received != REPORT_LEN) printk(KERN_WARNING "Device data transfer failed.\n");

    // Step two - Attempt to get the data out
    // We've prepared an empty buffer, and the device will populate it
    //  0x01 --> HID_REQ_GET_REPORT
    //  0XA1 --> USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN0x00, 0x86, 0x02
    memset(request, 0, sizeof(struct razer_report));
    received = usb_control_msg(tartarus, usb_sndctrlpipe(tartarus, 0), 0x01, 0XA1, 0x300, REPORT_IDX, request, REPORT_LEN, USB_CTRL_SET_TIMEOUT);

    // TODO CHECK FOR ERRORS AGAIN! (received != <expected size>)
    if (received != 90) printk(KERN_WARNING "Device data transfer failed.\n");

    // We aren't referencing the buffer so we copy it to the stack (rename if response var is used)
    memcpy(&response, request, sizeof(struct razer_report));

    // Final cleanup
    kfree(request);
    return response;
}

// Called when the device is bound
// Reference razerkbd_driver.c - Line 3184
static int tartarus_probe(struct hid_device* dev, const struct hid_device_id* id) {
    // TODO Add interface num
    printk(KERN_INFO "USB Driver Bound:  Vendor ID: 0x%02x  Product ID: 0x%02x\n", id->vendor, id->product);

    // struct usb_interface interface = to_usb_interface(dev->dev.parent);
    // set up a device struct and device files if any

    // Some device interfacing tests!
    int en = 0;
    struct razer_report cmd = generate_report(CMD_KBD_LAYOUT);
    struct razer_report out = send_command(dev, &cmd, &en);
    log_report(&out);

    return 0;
}

// Called when the device is unbound
static void tartarus_disconnect(struct hid_device* dev) {
    // Debugging output
    printk(KERN_INFO "USB Driver Unbound (Tartarus)\n");

}

// Called for every standard device event
// (Still unsure what is standard vs raw and when either is triggered)
static int event_handler(struct hid_device* dev, struct hid_field* field, struct hid_usage* usage, int32_t value) {
    printk(KERN_INFO "Event handler called :)\n");
    return 0;
}

// Called for every raw device event
static int raw_event_handler(struct hid_device* dev, struct hid_report* report, u8* data, int size) {
    printk(KERN_INFO "Raw event handler called :O\n");
    return 0;
}