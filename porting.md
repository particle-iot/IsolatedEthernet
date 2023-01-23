# Notes for porting WIZnet ioLibrary

- [WIZnet ioLibrary driver](https://github.com/Wiznet/ioLibrary_Driver)


- Ported ioLibrary v4.0.0 (2018-03-29). As of 2022-01-15, this was the latest version so clearly it doesn't change often.
- Rename .c files to .cpp as the cloud compiler doesn't compile C files, also because Wiring library to access SPI requires C++.
- Copy socket.* and wizchip_conf.* from Ethernet directory.
- Copy the W5500 directory from the Ethernet directory. Keep the subdir because that's how the header is accessed.
- Copy Internet/DHCP/* to src directory, renaming dhcp.c to dhcp.cpp
- Copy Internet/DNS/* to src directory, renaming dns.c to dns.cpp.
- Updated socket.cpp and socket.h to add a `namespace wiznet` to avoid conflicts with Device OS defintions of close(), etc.
- Updated dhcp.cpp and dns.cpp to add a `using namepsace wiznet` to use the correct socket definitions
- Added wizchip_debug() to socket.h to make it easier to add debug log messages.
- Changed printf to wizchip_debug in dhcp.cpp and dns.cpp to enable debug logs
- Added wizchip_yield() to socket.h and dns.cpp so DNS can yield CPU while blocking
- Also in socket.cpp during connect()
