#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>      // This import might not be needed
#include <linux/hid.h>
// #include <linux/usb/input.h>
// #include <linux/slab.h>
// #include <linux/dmi.h>


/*/ MISCELLANEOUS THINGS I MIGHT NEED

// usb_device struct reference
https://docs.kernel.org/driver-api/usb/usb.html#c.usb_device


// Not sure what this struct is yet
struct razer_kbd_device {
    unsigned int fn_on;
    DECLARE_BITMAP(pressed_fn, KEY_CNT);	// Pretty sure this reduces to an array of unsigned longs

    unsigned char block_keys[3];
    unsigned char left_alt_on;
};


// Macro for file creation -- Move this to compiled code if needed
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


// Driver specifciation struct
static struct usb_driver skel_driver = {
     name:        "skeleton",
     probe:       skel_probe,
     disconnect:  skel_disconnect,
     fops:        &skel_fops,
     minor:       USB_SKEL_MINOR_BASE,
     id_table:    skel_table,
};


// Report Responses -- DEFINITELY NEED THESE
#define RAZER_CMD_BUSY          0x01
#define RAZER_CMD_SUCCESSFUL    0x02
#define RAZER_CMD_FAILURE       0x03
#define RAZER_CMD_TIMEOUT       0x04
#define RAZER_CMD_NOT_SUPPORTED 0x05


struct razer_rgb {
    unsigned char r,g,b;
};

union transaction_id_union {
    unsigned char id;
    struct transaction_parts {
        unsigned char device : 3;
        unsigned char id : 5;
    } parts;
};

union command_id_union {
    unsigned char id;
    struct command_id_parts {
        unsigned char direction : 1;
        unsigned char id : 7;
    } parts;
};


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

static int tartarus_probe(struct hid_device*, const struct hid_device_id*);
static void tartarus_disconnect(struct hid_device*);

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
    // Probably would refer to the structure of this struct, but razer uses its own so it's obsolete?
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


// -- DEVICE INTERFACING (main driver goodness) --
// Called when the device is bound
static int tartarus_probe(struct hid_device* dev, const struct hid_device_id* id) {
    // razerkbd_driver.c - Line 3184
    // Might need to take in an hid_device instead?
    // dev_info(&interface->dev, "HELLO WORLD! USB Driver Probed: Vendor ID : 0x%02x,\t"
    //          "Product ID : 0x%02x\n", id->idVendor, id->idProduct);
    printk(KERN_INFO "HELLO WORLD!\n");
    return 0;
}

// Called when the device is unbound
static void tartarus_disconnect(struct hid_device* dev) {

}


// Just learned that this is all old kernel stuff
//
// -- Init/Exit functions (cannot find these in openrazer driver?) --
// Called when the driver is loaded (insmod / on boot)?
// static int __init tartarus_init(void) {
//     // Register the driver with the USB core
// 	int status = usb_register(&tartarus_driver);

//     // Verbose output for debugging
//     if (!status) printk(KERN_INFO "Tartarus driver successfully initalized!\n");
//     else printk(KERN_WARNING "Failed to register tartarus driver\n");

// 	return status;
// }

// // Called when the driver is unloaded (rmmod / on shutdown)?
// static void __exit tartarus_exit(void) {
// 	usb_deregister(&tartarus_driver);
// }

// module_init(tartarus_init);
// module_exit(tartarus_exit);
