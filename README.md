# USB password manager
## Overview
This USB device is able to generate and store a secure password with up to 256 digits (set to 100 digits by default). The password manager is able to identify itself as USB HID keyboard, therefore it works with any PC and OS without having to install further drivers.

## How to use
#### Generating the password
When first setting up the USB password manager or wanting to change the password, a new password has to be generated. To do so, plug the device into an USB port, open an new textfile and press the on-board button for approximately 3 seconds. The instructions for generating and storing a new password and thus deleting the previous one will be typed in asking you for confirmation via toggling the Caps Lock key.

#### Using the password
To access the stored password, plug the device into an USB port and toggle Caps Lock twice, the stored password followed by a return (depending on your settings) will be typed in.

## Reference
The USB password manager includes the [V-USB code](https://www.obdev.at/products/vusb/) by Objective Developement to emulate a USB HID keyboard to retrieve the Caps Lock state and type in the instructions and password. To generate the password, [this approach](https://gist.github.com/endolith/2568571) of an AVR hardware random generator by [endolith](https://github.com/endolith) was used.

## License
As the V-USB code, this library is licensed under GNU GPLv3, see [`LICENSE.txt`](LICENSE.txt) for further information.

## Todo
- add PCB design files
- add built instructions
- add pictures
- add security evaluations
