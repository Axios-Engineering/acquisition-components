#include <iostream>
#include "ossie/ossieSupport.h"

#include "RTLTcpSource.h"

 int main(int argc, char* argv[])
{
    RTLTcpSource_i* RTLTcpSource_servant;
    Resource_impl::start_component(RTLTcpSource_servant, argc, argv);
}
