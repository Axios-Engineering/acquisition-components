#ifndef RTLTCPSOURCE_IMPL_BASE_H
#define RTLTCPSOURCE_IMPL_BASE_H

#include <boost/thread.hpp>
#include <ossie/Resource_impl.h>

#include "port_impl.h"

#define NOOP 0
#define FINISH -1
#define NORMAL 1

class RTLTcpSource_base;


template < typename TargetClass >
class ProcessThread
{
    public:
        ProcessThread(TargetClass *_target, float _delay) :
            target(_target)
        {
            _mythread = 0;
            _thread_running = false;
            _udelay = (__useconds_t)(_delay * 1000000);
        };

        // kick off the thread
        void start() {
            if (_mythread == 0) {
                _thread_running = true;
                _mythread = new boost::thread(&ProcessThread::run, this);
            }
        };

        // manage calls to target's service function
        void run() {
            int state = NORMAL;
            while (_thread_running and (state != FINISH)) {
                state = target->serviceFunction();
                if (state == NOOP) usleep(_udelay);
            }
        };

        // stop thread and wait for termination
        bool release(unsigned long secs = 0, unsigned long usecs = 0) {
            _thread_running = false;
            if (_mythread)  {
                if ((secs == 0) and (usecs == 0)){
                    _mythread->join();
                } else {
                    boost::system_time waitime= boost::get_system_time() + boost::posix_time::seconds(secs) +  boost::posix_time::microseconds(usecs) ;
                    if (!_mythread->timed_join(waitime)) {
                        return 0;
                    }
                }
                delete _mythread;
                _mythread = 0;
            }
    
            return 1;
        };

        virtual ~ProcessThread(){
            if (_mythread != 0) {
                release(0);
                _mythread = 0;
            }
        };

        void updateDelay(float _delay) { _udelay = (__useconds_t)(_delay * 1000000); };

    private:
        boost::thread *_mythread;
        bool _thread_running;
        TargetClass *target;
        __useconds_t _udelay;
        boost::condition_variable _end_of_run;
        boost::mutex _eor_mutex;
};

class RTLTcpSource_base : public Resource_impl
{
    friend class BULKIO_dataShort_Out_i;
    friend class BULKIO_dataFloat_Out_i;
    friend class BULKIO_dataOctet_Out_i;

    public: 
        RTLTcpSource_base(const char *uuid, const char *label);

        void start() throw (CF::Resource::StartError, CORBA::SystemException);

        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

        CORBA::Object_ptr getPort(const char* _id) throw (CF::PortSupplier::UnknownPort, CORBA::SystemException);

        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

        void initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException);

        void loadProperties();

        virtual int serviceFunction() = 0;

    protected:
        ProcessThread<RTLTcpSource_base> *serviceThread; 
        boost::mutex serviceThreadLock;  

        // Member variables exposed as properties
        std::string rtl_host;
        CORBA::LongLong rtl_port;
        CORBA::ULong output_block_size;
        CORBA::ULongLong frequency;
        CORBA::ULongLong sample_rate;
        unsigned short gain_mode;
        CORBA::ULong gain;
        CORBA::Long frequency_correction;
        unsigned short agc_mode;
        unsigned short direct_sampling;
        unsigned short offset_tuning;
        std::vector<unsigned short> if_gain;
        std::string streamID;

        // Ports
        BULKIO_dataFloat_Out_i *ComplexIQ_Float;
        BULKIO_dataShort_Out_i *ComplexIQ_Short;
        BULKIO_dataOctet_Out_i *ComplexIQ_uByte;
    
    private:
        void construct();

};
#endif
