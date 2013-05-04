
#include "RTLTcpSource_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY
    
 	Source: RTLTcpSource.spd.xml
 	Generated on: Sat May 04 12:52:32 BST 2013
 	REDHAWK IDE
 	Version: R.1.8.3
 	Build id: v201303122306

*******************************************************************************************/

/******************************************************************************************

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/
 
RTLTcpSource_base::RTLTcpSource_base(const char *uuid, const char *label) :
                                     Resource_impl(uuid, label), serviceThread(0) {
    construct();
}

void RTLTcpSource_base::construct()
{
    Resource_impl::_started = false;
    loadProperties();
    serviceThread = 0;
    
    PortableServer::ObjectId_var oid;
    ComplexIQ_Float = new BULKIO_dataFloat_Out_i("ComplexIQ_Float", this);
    oid = ossie::corba::RootPOA()->activate_object(ComplexIQ_Float);
    ComplexIQ_Short = new BULKIO_dataShort_Out_i("ComplexIQ_Short", this);
    oid = ossie::corba::RootPOA()->activate_object(ComplexIQ_Short);
    ComplexIQ_uByte = new BULKIO_dataOctet_Out_i("ComplexIQ_uByte", this);
    oid = ossie::corba::RootPOA()->activate_object(ComplexIQ_uByte);

    registerOutPort(ComplexIQ_Float, ComplexIQ_Float->_this());
    registerOutPort(ComplexIQ_Short, ComplexIQ_Short->_this());
    registerOutPort(ComplexIQ_uByte, ComplexIQ_uByte->_this());
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void RTLTcpSource_base::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
}

void RTLTcpSource_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    if (serviceThread == 0) {
        serviceThread = new ProcessThread<RTLTcpSource_base>(this, 0.1);
        serviceThread->start();
    }
    
    if (!Resource_impl::started()) {
    	Resource_impl::start();
    }
}

void RTLTcpSource_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    boost::mutex::scoped_lock lock(serviceThreadLock);
    // release the child thread (if it exists)
    if (serviceThread != 0) {
        if (!serviceThread->release(2)) {
            throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
        }
        serviceThread = 0;
    }
    
    if (Resource_impl::started()) {
    	Resource_impl::stop();
    }
}

CORBA::Object_ptr RTLTcpSource_base::getPort(const char* _id) throw (CORBA::SystemException, CF::PortSupplier::UnknownPort)
{

    std::map<std::string, Port_Provides_base_impl *>::iterator p_in = inPorts.find(std::string(_id));
    if (p_in != inPorts.end()) {

    }

    std::map<std::string, CF::Port_var>::iterator p_out = outPorts_var.find(std::string(_id));
    if (p_out != outPorts_var.end()) {
        return CF::Port::_duplicate(p_out->second);
    }

    throw (CF::PortSupplier::UnknownPort());
}

void RTLTcpSource_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
    // This function clears the component running condition so main shuts down everything
    try {
        stop();
    } catch (CF::Resource::StopError& ex) {
        // TODO - this should probably be logged instead of ignored
    }

    // deactivate ports
    releaseInPorts();
    releaseOutPorts();

    delete(ComplexIQ_Float);
    delete(ComplexIQ_Short);
    delete(ComplexIQ_uByte);
 
    Resource_impl::releaseObject();
}

void RTLTcpSource_base::loadProperties()
{
    addProperty(rtl_host,
                "127.0.0.1", 
               "rtl_host",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(rtl_port,
                1234, 
               "rtl_port",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(output_block_size,
                262144, 
               "output_block_size",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(frequency,
                100000000, 
               "frequency",
               "",
               "readwrite",
               "Hz",
               "external",
               "configure");

    addProperty(sample_rate,
                2048000, 
               "sample_rate",
               "",
               "readwrite",
               "Hz",
               "external",
               "configure");

    addProperty(gain_mode,
                0, 
               "gain_mode",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(gain,
                0, 
               "gain",
               "",
               "readwrite",
               ".1dB",
               "external",
               "configure");

    addProperty(frequency_correction,
                0, 
               "frequency_correction",
               "",
               "readwrite",
               "ppm",
               "external",
               "configure");

    addProperty(agc_mode,
                1, 
               "agc_mode",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(direct_sampling,
                0, 
               "direct_sampling",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(offset_tuning,
                0, 
               "offset_tuning",
               "",
               "readwrite",
               "",
               "external",
               "configure");

    addProperty(if_gain,
               "if_gain",
               "",
               "readwrite",
               ".1dB",
               "external",
               "configure");

    addProperty(streamID,
               "streamID",
               "",
               "readwrite",
               "",
               "external",
               "configure");

}
