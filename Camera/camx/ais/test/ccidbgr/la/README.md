Application to perform i2c read / write using the cci interface

Usage:

ccidbgr -dev=[slotId]     slotId is the sensor slot id to be used. Default is 0.

1. Ensure ais_server is running

2. Follow the menu options.
2a. First set slave address [8bit format], address type [# of bytes], and data type [# of bytes] through cci_update
2b. Use read/write commands

3. You can change slave address through cci_update

