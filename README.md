# CWS-pH-driver-Linux
Driver for a CWS pH sensor coded for Linux with an abstraction layer that emulates the Costof2


### Set up the project ###

```bash
$ git clone https://github.com/EnocMartinez/CWS-pH-driver-Linux
$ cd CWS-pH-driver-Linux
$ make
```

To run it:

```bash
$ ./driver 
```

That's it! Notice that the serial port is hardcoded to `/dev/ttyUSB0`.