
#ifndef PORT_H
#define PORT_H

#include "ossie/Port_impl.h"
#include <queue>
#include <list>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>

class RTLTcpSource_base;
class RTLTcpSource_i;

#define CORBA_MAX_TRANSFER_BYTES omniORB::giopMaxMsgSize()


#include "BULKIO/bio_dataFloat.h"

#include "BULKIO/bio_dataShort.h"

#include "BULKIO/bio_dataOctet.h"

// ----------------------------------------------------------------------------------------
// BULKIO_dataShort_Out_i declaration
// ----------------------------------------------------------------------------------------
class BULKIO_dataShort_Out_i : public Port_Uses_base_impl, public virtual POA_BULKIO::UsesPortStatisticsProvider
{
    public:
        BULKIO_dataShort_Out_i(std::string port_name, RTLTcpSource_base *_parent);
        ~BULKIO_dataShort_Out_i();

        void pushSRI(const BULKIO::StreamSRI& H);
        
        /*
         * pushPacket
         *     description: push data out of the port
         *
         *  data: structure containing the payload to send out
         *  T: constant of type BULKIO::PrecisionUTCTime containing the timestamp for the outgoing data.
         *    tcmode: timecode mode
         *    tcstatus: timecode status 
         *    toff: fractional sample offset
         *    twsec: J1970 GMT 
         *    tfsec: fractional seconds: 0.0 to 1.0
         *  EOS: end-of-stream flag
         *  streamID: stream identifier
         */
        template <typename ALLOCATOR>
        void pushPacket(std::vector<CORBA::Short, ALLOCATOR>& data, BULKIO::PrecisionUTCTime& T, bool EOS, const std::string& streamID) {
            if (refreshSRI) {
                if (currentSRIs.find(streamID) == currentSRIs.end()) {
                    BULKIO::StreamSRI sri;
                    sri.hversion = 1;
                    sri.xstart = 0.0;
                    sri.xdelta = 1.0;
                    sri.xunits = BULKIO::UNITS_TIME;
                    sri.subsize = 0;
                    sri.ystart = 0.0;
                    sri.ydelta = 0.0;
                    sri.yunits = BULKIO::UNITS_NONE;
                    sri.mode = 0;
                    sri.streamID = streamID.c_str();
                    currentSRIs[streamID] = sri;
                }
                pushSRI(currentSRIs[streamID]);
            }
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            // Magic is below, make a new sequence using the data from the Iterator
            // as the data for the sequence.  The 'false' at the end is whether or not
            // CORBA is allowed to delete the buffer when the sequence is destroyed.
            PortTypes::ShortSequence seq = PortTypes::ShortSequence(data.size(), data.size(), &(data[0]), false);
            if (active) {
                std::vector < std::pair < BULKIO::dataShort_var, std::string > >::iterator port;
                for (port = outConnections.begin(); port != outConnections.end(); port++) {
                    try {
                        ((*port).first)->pushPacket(seq, T, EOS, streamID.c_str());
                        stats[(*port).second].update(data.size(), 0, 0, streamID);
                    } catch(...) {
                        std::cout << "Call to pushPacket by BULKIO_dataShort_Out_i failed" << std::endl;
                    }
                }
            }
        };
        class linkStatistics
        {
            public:
                struct statPoint {
                    unsigned int elements;
                    float queueSize;
                    double secs;
                    double usecs;
                };
                
                linkStatistics() {
                    bitSize = sizeof(CORBA::Short) * 8.0;
                    historyWindow = 10;
                    activeStreamIDs.resize(0);
                    receivedStatistics_idx = 0;
                    receivedStatistics.resize(historyWindow);
                    runningStats.elementsPerSecond = -1.0;
                    runningStats.bitsPerSecond = -1.0;
                    runningStats.callsPerSecond = -1.0;
                    runningStats.averageQueueDepth = -1.0;
                    runningStats.streamIDs.length(0);
                    runningStats.timeSinceLastCall = -1;
                    enabled = true;
                };

                void setEnabled(bool enableStats) {
                    enabled = enableStats;
                }

                void update(unsigned int elementsReceived, float queueSize, bool EOS, std::string streamID) {
                    if (!enabled) {
                        return;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);
                    receivedStatistics[receivedStatistics_idx].elements = elementsReceived;
                    receivedStatistics[receivedStatistics_idx].queueSize = queueSize;
                    receivedStatistics[receivedStatistics_idx].secs = tv.tv_sec;
                    receivedStatistics[receivedStatistics_idx++].usecs = tv.tv_usec;
                    receivedStatistics_idx = receivedStatistics_idx % historyWindow;
                    if (!EOS) {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        bool foundStreamID = false;
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                foundStreamID = true;
                                break;
                            }
                            p++;
                        }
                        if (!foundStreamID) {
                            activeStreamIDs.push_back(streamID);
                        }
                    } else {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                activeStreamIDs.erase(p);
                                break;
                            }
                            p++;
                        }
                    }
                };

                BULKIO::PortStatistics retrieve() {
                    if (!enabled) {
                        return runningStats;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);

                    int idx = (receivedStatistics_idx == 0) ? (historyWindow - 1) : (receivedStatistics_idx - 1);
                    double front_sec = receivedStatistics[idx].secs;
                    double front_usec = receivedStatistics[idx].usecs;
                    double secDiff = tv.tv_sec - receivedStatistics[receivedStatistics_idx].secs;
                    double usecDiff = (tv.tv_usec - receivedStatistics[receivedStatistics_idx].usecs) / ((double)1e6);

                    double totalTime = secDiff + usecDiff;
                    double totalData = 0;
                    float queueSize = 0;
                    int startIdx = (receivedStatistics_idx + 1) % historyWindow;
                    for (int i = startIdx; i != receivedStatistics_idx; ) {
                        totalData += receivedStatistics[i].elements;
                        queueSize += receivedStatistics[i].queueSize;
                        i = (i + 1) % historyWindow;
                    }
                    runningStats.bitsPerSecond = ((totalData * bitSize) / totalTime);
                    runningStats.elementsPerSecond = (totalData / totalTime);
                    runningStats.averageQueueDepth = (queueSize / historyWindow);
                    runningStats.callsPerSecond = (double(historyWindow - 1) / totalTime);
                    runningStats.timeSinceLastCall = (((double)tv.tv_sec) - front_sec) + (((double)tv.tv_usec - front_usec) / ((double)1e6));
                    unsigned int streamIDsize = activeStreamIDs.size();
                    std::list< std::string >::iterator p = activeStreamIDs.begin();
                    runningStats.streamIDs.length(streamIDsize);
                    for (unsigned int i = 0; i < streamIDsize; i++) {
                        if (p == activeStreamIDs.end()) {
                            break;
                        }
                        runningStats.streamIDs[i] = CORBA::string_dup((*p).c_str());
                        p++;
                    }
                    return runningStats;
                };

            protected:
                bool enabled;
                double bitSize;
                BULKIO::PortStatistics runningStats;
                std::vector<statPoint> receivedStatistics;
                std::list< std::string > activeStreamIDs;
                unsigned long historyWindow;
                int receivedStatistics_idx;
        };

        BULKIO::UsesPortStatisticsSequence * statistics()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            BULKIO::UsesPortStatisticsSequence_var recStat = new BULKIO::UsesPortStatisticsSequence();
            recStat->length(outConnections.size());
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                recStat[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                recStat[i].statistics = stats[outConnections[i].second].retrieve();
            }
            return recStat._retn();
        };

        BULKIO::PortUsageType state()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            if (outConnections.size() > 0) {
                return BULKIO::ACTIVE;
            } else {
                return BULKIO::IDLE;
            }

            return BULKIO::BUSY;
        };
        
        void enableStats(bool enable)
        {
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                stats[outConnections[i].second].setEnabled(enable);
            }
        };


        ExtendedCF::UsesConnectionSequence * connections() 
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            if (recConnectionsRefresh) {
                recConnections.length(outConnections.size());
                for (unsigned int i = 0; i < outConnections.size(); i++) {
                    recConnections[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                    recConnections[i].port = CORBA::Object::_duplicate(outConnections[i].first);
                }
                recConnectionsRefresh = false;
            }
            ExtendedCF::UsesConnectionSequence_var retVal = new ExtendedCF::UsesConnectionSequence(recConnections);
            // NOTE: You must delete the object that this function returns!
            return retVal._retn();
        };

        void connectPort(CORBA::Object_ptr connection, const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            BULKIO::dataShort_var port = BULKIO::dataShort::_narrow(connection);
            outConnections.push_back(std::make_pair(port, connectionId));
            active = true;
            recConnectionsRefresh = true;
            refreshSRI = true;
        };

        void disconnectPort(const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                if (outConnections[i].second == connectionId) {
                    outConnections.erase(outConnections.begin() + i);
                    break;
                }
            }

            if (outConnections.size() == 0) {
                active = false;
            }
            recConnectionsRefresh = true;
        };

        std::vector< std::pair<BULKIO::dataShort_var, std::string> > _getConnections()
        {
            return outConnections;
        };
        std::map<std::string, BULKIO::StreamSRI> currentSRIs;

    protected:
        RTLTcpSource_i *parent;
        std::vector < std::pair<BULKIO::dataShort_var, std::string> > outConnections;
        ExtendedCF::UsesConnectionSequence recConnections;
        bool recConnectionsRefresh;
        std::map<std::string, linkStatistics> stats;
};

// ----------------------------------------------------------------------------------------
// BULKIO_dataFloat_Out_i declaration
// ----------------------------------------------------------------------------------------
class BULKIO_dataFloat_Out_i : public Port_Uses_base_impl, public virtual POA_BULKIO::UsesPortStatisticsProvider
{
    public:
        BULKIO_dataFloat_Out_i(std::string port_name, RTLTcpSource_base *_parent);
        ~BULKIO_dataFloat_Out_i();

        void pushSRI(const BULKIO::StreamSRI& H);
        
        /*
         * pushPacket
         *     description: push data out of the port
         *
         *  data: structure containing the payload to send out
         *  T: constant of type BULKIO::PrecisionUTCTime containing the timestamp for the outgoing data.
         *    tcmode: timecode mode
         *    tcstatus: timecode status 
         *    toff: fractional sample offset
         *    twsec: J1970 GMT 
         *    tfsec: fractional seconds: 0.0 to 1.0
         *  EOS: end-of-stream flag
         *  streamID: stream identifier
         */
        template <typename ALLOCATOR>
        void pushPacket(std::vector<CORBA::Float, ALLOCATOR>& data, BULKIO::PrecisionUTCTime& T, bool EOS, const std::string& streamID) {
            if (refreshSRI) {
                if (currentSRIs.find(streamID) == currentSRIs.end()) {
                    BULKIO::StreamSRI sri;
                    sri.hversion = 1;
                    sri.xstart = 0.0;
                    sri.xdelta = 1.0;
                    sri.xunits = BULKIO::UNITS_TIME;
                    sri.subsize = 0;
                    sri.ystart = 0.0;
                    sri.ydelta = 0.0;
                    sri.yunits = BULKIO::UNITS_NONE;
                    sri.mode = 0;
                    sri.streamID = streamID.c_str();
                    currentSRIs[streamID] = sri;
                }
                pushSRI(currentSRIs[streamID]);
            }
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            // Magic is below, make a new sequence using the data from the Iterator
            // as the data for the sequence.  The 'false' at the end is whether or not
            // CORBA is allowed to delete the buffer when the sequence is destroyed.
            PortTypes::FloatSequence seq = PortTypes::FloatSequence(data.size(), data.size(), &(data[0]), false);
            if (active) {
                std::vector < std::pair < BULKIO::dataFloat_var, std::string > >::iterator port;
                for (port = outConnections.begin(); port != outConnections.end(); port++) {
                    try {
                        ((*port).first)->pushPacket(seq, T, EOS, streamID.c_str());
                        stats[(*port).second].update(data.size(), 0, 0, streamID);
                    } catch(...) {
                        std::cout << "Call to pushPacket by BULKIO_dataFloat_Out_i failed" << std::endl;
                    }
                }
            }
        };
        class linkStatistics
        {
            public:
                struct statPoint {
                    unsigned int elements;
                    float queueSize;
                    double secs;
                    double usecs;
                };
                
                linkStatistics() {
                    bitSize = sizeof(CORBA::Float) * 8.0;
                    historyWindow = 10;
                    activeStreamIDs.resize(0);
                    receivedStatistics_idx = 0;
                    receivedStatistics.resize(historyWindow);
                    runningStats.elementsPerSecond = -1.0;
                    runningStats.bitsPerSecond = -1.0;
                    runningStats.callsPerSecond = -1.0;
                    runningStats.averageQueueDepth = -1.0;
                    runningStats.streamIDs.length(0);
                    runningStats.timeSinceLastCall = -1;
                    enabled = true;
                };

                void setEnabled(bool enableStats) {
                    enabled = enableStats;
                }

                void update(unsigned int elementsReceived, float queueSize, bool EOS, std::string streamID) {
                    if (!enabled) {
                        return;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);
                    receivedStatistics[receivedStatistics_idx].elements = elementsReceived;
                    receivedStatistics[receivedStatistics_idx].queueSize = queueSize;
                    receivedStatistics[receivedStatistics_idx].secs = tv.tv_sec;
                    receivedStatistics[receivedStatistics_idx++].usecs = tv.tv_usec;
                    receivedStatistics_idx = receivedStatistics_idx % historyWindow;
                    if (!EOS) {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        bool foundStreamID = false;
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                foundStreamID = true;
                                break;
                            }
                            p++;
                        }
                        if (!foundStreamID) {
                            activeStreamIDs.push_back(streamID);
                        }
                    } else {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                activeStreamIDs.erase(p);
                                break;
                            }
                            p++;
                        }
                    }
                };

                BULKIO::PortStatistics retrieve() {
                    if (!enabled) {
                        return runningStats;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);

                    int idx = (receivedStatistics_idx == 0) ? (historyWindow - 1) : (receivedStatistics_idx - 1);
                    double front_sec = receivedStatistics[idx].secs;
                    double front_usec = receivedStatistics[idx].usecs;
                    double secDiff = tv.tv_sec - receivedStatistics[receivedStatistics_idx].secs;
                    double usecDiff = (tv.tv_usec - receivedStatistics[receivedStatistics_idx].usecs) / ((double)1e6);

                    double totalTime = secDiff + usecDiff;
                    double totalData = 0;
                    float queueSize = 0;
                    int startIdx = (receivedStatistics_idx + 1) % historyWindow;
                    for (int i = startIdx; i != receivedStatistics_idx; ) {
                        totalData += receivedStatistics[i].elements;
                        queueSize += receivedStatistics[i].queueSize;
                        i = (i + 1) % historyWindow;
                    }
                    runningStats.bitsPerSecond = ((totalData * bitSize) / totalTime);
                    runningStats.elementsPerSecond = (totalData / totalTime);
                    runningStats.averageQueueDepth = (queueSize / historyWindow);
                    runningStats.callsPerSecond = (double(historyWindow - 1) / totalTime);
                    runningStats.timeSinceLastCall = (((double)tv.tv_sec) - front_sec) + (((double)tv.tv_usec - front_usec) / ((double)1e6));
                    unsigned int streamIDsize = activeStreamIDs.size();
                    std::list< std::string >::iterator p = activeStreamIDs.begin();
                    runningStats.streamIDs.length(streamIDsize);
                    for (unsigned int i = 0; i < streamIDsize; i++) {
                        if (p == activeStreamIDs.end()) {
                            break;
                        }
                        runningStats.streamIDs[i] = CORBA::string_dup((*p).c_str());
                        p++;
                    }
                    return runningStats;
                };

            protected:
                bool enabled;
                double bitSize;
                BULKIO::PortStatistics runningStats;
                std::vector<statPoint> receivedStatistics;
                std::list< std::string > activeStreamIDs;
                unsigned long historyWindow;
                int receivedStatistics_idx;
        };

        BULKIO::UsesPortStatisticsSequence * statistics()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            BULKIO::UsesPortStatisticsSequence_var recStat = new BULKIO::UsesPortStatisticsSequence();
            recStat->length(outConnections.size());
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                recStat[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                recStat[i].statistics = stats[outConnections[i].second].retrieve();
            }
            return recStat._retn();
        };

        BULKIO::PortUsageType state()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            if (outConnections.size() > 0) {
                return BULKIO::ACTIVE;
            } else {
                return BULKIO::IDLE;
            }

            return BULKIO::BUSY;
        };
        
        void enableStats(bool enable)
        {
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                stats[outConnections[i].second].setEnabled(enable);
            }
        };


        ExtendedCF::UsesConnectionSequence * connections() 
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            if (recConnectionsRefresh) {
                recConnections.length(outConnections.size());
                for (unsigned int i = 0; i < outConnections.size(); i++) {
                    recConnections[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                    recConnections[i].port = CORBA::Object::_duplicate(outConnections[i].first);
                }
                recConnectionsRefresh = false;
            }
            ExtendedCF::UsesConnectionSequence_var retVal = new ExtendedCF::UsesConnectionSequence(recConnections);
            // NOTE: You must delete the object that this function returns!
            return retVal._retn();
        };

        void connectPort(CORBA::Object_ptr connection, const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            BULKIO::dataFloat_var port = BULKIO::dataFloat::_narrow(connection);
            outConnections.push_back(std::make_pair(port, connectionId));
            active = true;
            recConnectionsRefresh = true;
            refreshSRI = true;
        };

        void disconnectPort(const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                if (outConnections[i].second == connectionId) {
                    outConnections.erase(outConnections.begin() + i);
                    break;
                }
            }

            if (outConnections.size() == 0) {
                active = false;
            }
            recConnectionsRefresh = true;
        };

        std::vector< std::pair<BULKIO::dataFloat_var, std::string> > _getConnections()
        {
            return outConnections;
        };
        std::map<std::string, BULKIO::StreamSRI> currentSRIs;

    protected:
        RTLTcpSource_i *parent;
        std::vector < std::pair<BULKIO::dataFloat_var, std::string> > outConnections;
        ExtendedCF::UsesConnectionSequence recConnections;
        bool recConnectionsRefresh;
        std::map<std::string, linkStatistics> stats;
};

// ----------------------------------------------------------------------------------------
// BULKIO_dataOctet_Out_i declaration
// ----------------------------------------------------------------------------------------
class BULKIO_dataOctet_Out_i : public Port_Uses_base_impl, public virtual POA_BULKIO::UsesPortStatisticsProvider
{
    public:
        BULKIO_dataOctet_Out_i(std::string port_name, RTLTcpSource_base *_parent);
        ~BULKIO_dataOctet_Out_i();

        void pushSRI(const BULKIO::StreamSRI& H);
        
        /*
         * pushPacket
         *     description: push data out of the port
         *
         *  data: structure containing the payload to send out
         *  T: constant of type BULKIO::PrecisionUTCTime containing the timestamp for the outgoing data.
         *    tcmode: timecode mode
         *    tcstatus: timecode status 
         *    toff: fractional sample offset
         *    twsec: J1970 GMT 
         *    tfsec: fractional seconds: 0.0 to 1.0
         *  EOS: end-of-stream flag
         *  streamID: stream identifier
         */
        template <typename ALLOCATOR>
        void pushPacket(std::vector<unsigned char, ALLOCATOR>& data, BULKIO::PrecisionUTCTime& T, bool EOS, const std::string& streamID) {
            if (refreshSRI) {
                if (currentSRIs.find(streamID) == currentSRIs.end()) {
                    BULKIO::StreamSRI sri;
                    sri.hversion = 1;
                    sri.xstart = 0.0;
                    sri.xdelta = 1.0;
                    sri.xunits = BULKIO::UNITS_TIME;
                    sri.subsize = 0;
                    sri.ystart = 0.0;
                    sri.ydelta = 0.0;
                    sri.yunits = BULKIO::UNITS_NONE;
                    sri.mode = 0;
                    sri.streamID = streamID.c_str();
                    currentSRIs[streamID] = sri;
                }
                pushSRI(currentSRIs[streamID]);
            }
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            // Magic is below, make a new sequence using the data from the Iterator
            // as the data for the sequence.  The 'false' at the end is whether or not
            // CORBA is allowed to delete the buffer when the sequence is destroyed.
            CF::OctetSequence seq = CF::OctetSequence(data.size(), data.size(), &(data[0]), false);
            if (active) {
                std::vector < std::pair < BULKIO::dataOctet_var, std::string > >::iterator port;
                for (port = outConnections.begin(); port != outConnections.end(); port++) {
                    try {
                        ((*port).first)->pushPacket(seq, T, EOS, streamID.c_str());
                        stats[(*port).second].update(data.size(), 0, 0, streamID);
                    } catch(...) {
                        std::cout << "Call to pushPacket by BULKIO_dataOctet_Out_i failed" << std::endl;
                    }
                }
            }
        };
        class linkStatistics
        {
            public:
                struct statPoint {
                    unsigned int elements;
                    float queueSize;
                    double secs;
                    double usecs;
                };
                
                linkStatistics() {
                    bitSize = sizeof(unsigned char) * 8.0;
                    historyWindow = 10;
                    activeStreamIDs.resize(0);
                    receivedStatistics_idx = 0;
                    receivedStatistics.resize(historyWindow);
                    runningStats.elementsPerSecond = -1.0;
                    runningStats.bitsPerSecond = -1.0;
                    runningStats.callsPerSecond = -1.0;
                    runningStats.averageQueueDepth = -1.0;
                    runningStats.streamIDs.length(0);
                    runningStats.timeSinceLastCall = -1;
                    enabled = true;
                };

                void setEnabled(bool enableStats) {
                    enabled = enableStats;
                }

                void update(unsigned int elementsReceived, float queueSize, bool EOS, std::string streamID) {
                    if (!enabled) {
                        return;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);
                    receivedStatistics[receivedStatistics_idx].elements = elementsReceived;
                    receivedStatistics[receivedStatistics_idx].queueSize = queueSize;
                    receivedStatistics[receivedStatistics_idx].secs = tv.tv_sec;
                    receivedStatistics[receivedStatistics_idx++].usecs = tv.tv_usec;
                    receivedStatistics_idx = receivedStatistics_idx % historyWindow;
                    if (!EOS) {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        bool foundStreamID = false;
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                foundStreamID = true;
                                break;
                            }
                            p++;
                        }
                        if (!foundStreamID) {
                            activeStreamIDs.push_back(streamID);
                        }
                    } else {
                        std::list<std::string>::iterator p = activeStreamIDs.begin();
                        while (p != activeStreamIDs.end()) {
                            if (*p == streamID) {
                                activeStreamIDs.erase(p);
                                break;
                            }
                            p++;
                        }
                    }
                };

                BULKIO::PortStatistics retrieve() {
                    if (!enabled) {
                        return runningStats;
                    }
                    struct timeval tv;
                    struct timezone tz;
                    gettimeofday(&tv, &tz);

                    int idx = (receivedStatistics_idx == 0) ? (historyWindow - 1) : (receivedStatistics_idx - 1);
                    double front_sec = receivedStatistics[idx].secs;
                    double front_usec = receivedStatistics[idx].usecs;
                    double secDiff = tv.tv_sec - receivedStatistics[receivedStatistics_idx].secs;
                    double usecDiff = (tv.tv_usec - receivedStatistics[receivedStatistics_idx].usecs) / ((double)1e6);

                    double totalTime = secDiff + usecDiff;
                    double totalData = 0;
                    float queueSize = 0;
                    int startIdx = (receivedStatistics_idx + 1) % historyWindow;
                    for (int i = startIdx; i != receivedStatistics_idx; ) {
                        totalData += receivedStatistics[i].elements;
                        queueSize += receivedStatistics[i].queueSize;
                        i = (i + 1) % historyWindow;
                    }
                    runningStats.bitsPerSecond = ((totalData * bitSize) / totalTime);
                    runningStats.elementsPerSecond = (totalData / totalTime);
                    runningStats.averageQueueDepth = (queueSize / historyWindow);
                    runningStats.callsPerSecond = (double(historyWindow - 1) / totalTime);
                    runningStats.timeSinceLastCall = (((double)tv.tv_sec) - front_sec) + (((double)tv.tv_usec - front_usec) / ((double)1e6));
                    unsigned int streamIDsize = activeStreamIDs.size();
                    std::list< std::string >::iterator p = activeStreamIDs.begin();
                    runningStats.streamIDs.length(streamIDsize);
                    for (unsigned int i = 0; i < streamIDsize; i++) {
                        if (p == activeStreamIDs.end()) {
                            break;
                        }
                        runningStats.streamIDs[i] = CORBA::string_dup((*p).c_str());
                        p++;
                    }
                    return runningStats;
                };

            protected:
                bool enabled;
                double bitSize;
                BULKIO::PortStatistics runningStats;
                std::vector<statPoint> receivedStatistics;
                std::list< std::string > activeStreamIDs;
                unsigned long historyWindow;
                int receivedStatistics_idx;
        };

        BULKIO::UsesPortStatisticsSequence * statistics()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            BULKIO::UsesPortStatisticsSequence_var recStat = new BULKIO::UsesPortStatisticsSequence();
            recStat->length(outConnections.size());
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                recStat[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                recStat[i].statistics = stats[outConnections[i].second].retrieve();
            }
            return recStat._retn();
        };

        BULKIO::PortUsageType state()
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);
            if (outConnections.size() > 0) {
                return BULKIO::ACTIVE;
            } else {
                return BULKIO::IDLE;
            }

            return BULKIO::BUSY;
        };
        
        void enableStats(bool enable)
        {
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                stats[outConnections[i].second].setEnabled(enable);
            }
        };


        ExtendedCF::UsesConnectionSequence * connections() 
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            if (recConnectionsRefresh) {
                recConnections.length(outConnections.size());
                for (unsigned int i = 0; i < outConnections.size(); i++) {
                    recConnections[i].connectionId = CORBA::string_dup(outConnections[i].second.c_str());
                    recConnections[i].port = CORBA::Object::_duplicate(outConnections[i].first);
                }
                recConnectionsRefresh = false;
            }
            ExtendedCF::UsesConnectionSequence_var retVal = new ExtendedCF::UsesConnectionSequence(recConnections);
            // NOTE: You must delete the object that this function returns!
            return retVal._retn();
        };

        void connectPort(CORBA::Object_ptr connection, const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            BULKIO::dataOctet_var port = BULKIO::dataOctet::_narrow(connection);
            outConnections.push_back(std::make_pair(port, connectionId));
            active = true;
            recConnectionsRefresh = true;
            refreshSRI = true;
        };

        void disconnectPort(const char* connectionId)
        {
            boost::mutex::scoped_lock lock(updatingPortsLock);   // don't want to process while command information is coming in
            for (unsigned int i = 0; i < outConnections.size(); i++) {
                if (outConnections[i].second == connectionId) {
                    outConnections.erase(outConnections.begin() + i);
                    break;
                }
            }

            if (outConnections.size() == 0) {
                active = false;
            }
            recConnectionsRefresh = true;
        };

        std::vector< std::pair<BULKIO::dataOctet_var, std::string> > _getConnections()
        {
            return outConnections;
        };
        std::map<std::string, BULKIO::StreamSRI> currentSRIs;

    protected:
        RTLTcpSource_i *parent;
        std::vector < std::pair<BULKIO::dataOctet_var, std::string> > outConnections;
        ExtendedCF::UsesConnectionSequence recConnections;
        bool recConnectionsRefresh;
        std::map<std::string, linkStatistics> stats;
};
#endif
