acquisition-components
======================

Components that support radio IQ data acquisition.

![ScreenShot](https://github.com/Axios-Engineering/acquisition-components/raw/master/images/screenshot.png)

RTLTcpSource
---------------
Provides acquisition from the
[rtl_tcp](http://sdr.osmocom.org/trac/wiki/rtl-sdr#rtl_tcp) server that is part
of the RTL driver package.  You must first download and install the RTL drivers
from [here](http://sdr.osmocom.org/trac/wiki/rtl-sdr).

It is highly recommended that your verify your hardware using the `rtl_test`
program before attempting to use the RTLTcpSource. 

You must run the `rtl_tcp` program in order to use RTLTcpSource.
