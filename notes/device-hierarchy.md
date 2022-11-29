My current understanding of the hierarchy:
 usb_device is the full representation of the device to the kernel
 usb_interface is a "sub-device" of a usb_device
   The parent does not have a list of its interfaces, but instead
     its children are given pointers to their parent when detected by the kernel
   These represent the (possibly) multiple interfaces within the usb_device
 hid_device is a child of a usb_interface
```c
// Cast our HID device to a USB device
struct usb_interface* interface = to_usb_interface(dev->dev.parent);
struct usb_device* device = interface_to_usbdev(interface);
```