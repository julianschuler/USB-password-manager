# USB password manager
## Overview
This USB device based on the ATtiny85 is able to generate and store a secure password with up to 510 digits (set to 100 digits by default). The password manager is able to identify itself as USB HID keyboard, therefore it works with any PC and OS without having to install further drivers.

## How to use
#### Generating the password
When first setting up the USB password manager or wanting to change the password, a new password has to be generated. Keep in mind, that the previous password will be deleted when you a generating a new one! To do so, plug the device into an USB port, open an new textfile and press the on-board button for approximately 3 seconds. The instructions for generating and storing a new password and thus deleting the previous one will be typed in asking you for confirmation via toggling the Caps Lock key.

#### Using the password
To access the stored password, plug the device into an USB port and toggle Caps Lock twice, the stored password followed by a return (depending on your settings) will be typed in.

## Security evaluations
The security benefit of this project in comparison to ordinary passwords should be considered with a grain of salt: The generated password itself is way stronger than ordanary passwords due to their length and lack of using schemes vulnerable to dictionary attacs (names, dates etc.). Furthermore the random number generator used to generate the password relies on two asynchronus clocks that slightly differ on every AVR because of manufacturing tolerances, this source of entropy can be considered as fairly reliable. Still the USBB password manager should be handled like a key to your house: if it's getting lost or stolen, there are no further security measures stopping someone getting in your house respectively retrieving your password! In general, a password manager is an easier and more secure option and should be preferred, this device is considered for special cases or for example to enhance existing passwords by appending the stored password to your normal ones.

## Reference
The USB password manager includes the [V-USB code](https://www.obdev.at/products/vusb/) by Objective Developement to emulate a USB HID keyboard to retrieve the Caps Lock state and type in the instructions and password. To generate the password, [this approach](https://gist.github.com/endolith/2568571) of an AVR hardware random number generator by [endolith](https://github.com/endolith) was used.

## License
As the V-USB code, this project is licensed under GNU GPLv3, see [`LICENSE.txt`](LICENSE.txt) for further information.

## Todo
- add build instructions
- add pictures
