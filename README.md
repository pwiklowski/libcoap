# What is it ?
This repository contains c++ implementaion CoAP server/client. It was succesfully utlized in liboic library that can be found in my repositories on github.

It can be used using IPv4, IPv6 or any proprietary protocol. When using library you must provide "sender" method that will take COAPPacket and send it out and when respose will be received make sure that server.handlePacket(COAPPacker* p) will be called. 



# How to use it ?
```
cmake .
make
```
If you want more details how to use it take a look at liboic implementaion or wait for more detailed description how to use it.

# Requiremnts
- c++11
- https://github.com/pwiklowski/lightstdlib

# TODO:
- Provide more examples using IPv4, IPv6 and RFM69 radio modules.
- Create more detailed description how to use it

# License:
MIT

