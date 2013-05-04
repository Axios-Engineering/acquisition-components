/**
 * Copyright (C) 2013 Axios, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "RTLTcpSource.h"
#include <sstream>
PREPARE_LOGGING(RTLTcpSource_i)

RTLTcpSource_i::RTLTcpSource_i(const char *uuid, const char *label) :
    RTLTcpSource_base(uuid, label)
{
}

RTLTcpSource_i::~RTLTcpSource_i()
{
}

void RTLTcpSource_i::validate(CF::Properties property, CF::Properties& validProps, CF::Properties& invalidProps)
{
    // TODO validate frequency based off tuner type
    // RTLSDR_TUNER_E4000 52e6 - 2.2e9 with (temperature dependent) gap between 1100 to 1250 MHz
    // RTLSDR_TUNER_FC0012 22e6, 948e6
    // RTLSDR_TUNER_FC0013 22e6, 1.1e9
    // RTLSDR_TUNER_FC2580 146e6, 308e6 ; 438e6, 924e6
    // RTLSDR_TUNER_R820T 24e6, 1766e6
    // Default is E4000

    // TODO provide sample rate warnings for sample rates
    // that may not work (these are warnings not errors)
    // 250k, 1M 1.024M, 1.8M, 1.92M, 2M, 2.048M, 2.4M - Good
    // 2.6M, 2.8M, 3.0M, 3.2M - Warning might not work
    // Throw error if below 250k or above 3.2M

    // TODO validate gains (see the constants in RTLTcpSource

    // TODO validate if_gain (currently only supported by E4000
    // 3dB to 56db in .1 dB increments

    // TODO validate parameters that can't be set while started, aren't configured.
    if (started()) {
        // TODO rtl_host, rtl_port, streamID
    }
}

void RTLTcpSource_i::configure (const CF::Properties& configProperties)
    throw (CF::PropertySet::PartialConfiguration,
           CF::PropertySet::InvalidConfiguration,
           CORBA::SystemException)
{
    CF::Properties validProperties;
    CF::Properties invalidProperties;
    validate(configProperties, validProperties, invalidProperties);

    if (invalidProperties.length() > 0) {
        throw CF::PropertySet::InvalidConfiguration("Properties failed validation.  See log for details.", invalidProperties);
    }

    PropertySet_impl::configure(configProperties);
}

void RTLTcpSource_i::onconfigure_rtl_property(const std::string& id) {
    boost::mutex::scoped_lock lock(serviceThreadLock);

    if (_rtl_socket == -1) {
        LOG_DEBUG(RTLTcpSource_i, "No socket yet for configuration");
        return;
    }

    if (id == "*" || id == "frequency") {
        struct command cmd = { 0x01, htonl(frequency) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
        updateSRI();
    } else if (id == "*" || id == "sample_rate") {
        struct command cmd = { 0x02, htonl(sample_rate) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
        updateSRI();
    } else if (id == "*" || id == "gain_mode") {
        struct command cmd = { 0x03, htonl(gain_mode) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
        if (gain_mode == 1) { // If we have enabled manual gain, then send the current gain
            struct command cmd = { 0x04, htonl(gain) };
            send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
        }
    } else if (id == "*" || id == "gain") {
        if (gain_mode == 0) {
            LOG_WARN(RTLTcpSource_i, "Ignoring gain command in automatic gain mode");
        } else {
            struct command cmd = { 0x04, htonl(gain) };
            send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
        }
    } else if (id == "*" || id == "frequency_correction") {
        struct command cmd = { 0x05, htonl(frequency_correction) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
    } else if (id == "*" || id == "if_gain") {
        if (_tuner_if_gain_count == 0) {
            LOG_DEBUG(RTLTcpSource_i, "Ignoring if_gain for device that doesn't support it");
        } else {
            for (size_t stage=0; stage<if_gain.size(); stage++) {
                uint16_t gain = if_gain[stage];
                uint32_t params = (stage+1) << 16 | (gain & 0xffff);
                struct command cmd = { 0x06, htonl(params) };
                send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
            }
        }
    } else if (id == "*" || id == "agc_mode") {
        struct command cmd = { 0x08, htonl(agc_mode) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
    } else if (id == "*" || id == "direct_sampling") {
        struct command cmd = { 0x09, htonl(direct_sampling) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
    } else if (id == "*" || id == "offset_tuning") {
        struct command cmd = { 0x0a, htonl(offset_tuning) };
        send(_rtl_socket, (const char*)&cmd, sizeof(cmd), 0);
    }
}

void RTLTcpSource_i::initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException)
{
    setPropertyChangeListener("frequency", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("sample_rate", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("gain_mode", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("gain", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("frequency_correction", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("if_gain", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("agc_mode", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("direct_sampling", this, &RTLTcpSource_i::onconfigure_rtl_property);
    setPropertyChangeListener("offset_tuning", this, &RTLTcpSource_i::onconfigure_rtl_property);
}

void RTLTcpSource_i::releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException)
{

}

void RTLTcpSource_i::start() throw (CF::Resource::StartError, CORBA::SystemException)
{
    boost::recursive_mutex::scoped_lock lock(_componentLock);

    if (started()) {
        LOG_DEBUG(RTLTcpSource_i, "Device already started");
        return;
    }

    if (rtl_host == "") {
        LOG_ERROR(RTLTcpSource_i, "No host name has been provided");
        throw new CF::Resource::StartError(CF::CF_NOTSET, "No host name has been provided");
    }

    // Create some hints about the type of socket we want
    struct addrinfo hints;
    memset((void*) &hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Not entirely necessary, but do it for completeness
    std::stringstream service;
    service << rtl_port;

    LOG_INFO(RTLTcpSource_i, "Connecting to " << rtl_host << ":" << rtl_port);

    struct addrinfo *ai = 0;
    try {
        if (int ret = getaddrinfo( rtl_host.c_str(), service.str().c_str(), &hints, &ai )) {
            LOG_ERROR(RTLTcpSource_i, "Failed to get RTL device address: " << gai_strerror(ret));
            throw new CF::Resource::StartError(CF::CF_NOTSET, "Failed to get RTL device address");
        }

        _rtl_socket = -1;
        struct addrinfo *ip_src = ai;

        // create socket, trying all addresses returned (see getaddrinfo man page)
        while (ip_src != 0) {
            _rtl_socket = socket(ip_src->ai_family, ip_src->ai_socktype, ip_src->ai_protocol);
            if (_rtl_socket == -1) {
                LOG_DEBUG(RTLTcpSource_i, "Could not open socket, trying next one if available");
                ip_src = ip_src->ai_next;
            } else {
                break;
            }
        }

        if (_rtl_socket == -1) {
            LOG_ERROR(RTLTcpSource_i, "Failed to open RTL socket");
            throw new CF::Resource::StartError(CF::CF_NOTSET, "Failed to open RTL socket");
        }

        // Turn on reuse address
        int opt_val = 1;
        if (setsockopt(_rtl_socket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int)) == -1) {
            LOG_WARN(RTLTcpSource_i, "Cannot set socket SO_REUSEADDR");
        }

        // Don't wait when shutting down
        linger lngr;
        lngr.l_onoff = 1;
        lngr.l_linger = 0;
        if (setsockopt(_rtl_socket, SOL_SOCKET, SO_LINGER, &lngr, sizeof(linger)) == -1) {
            LOG_WARN(RTLTcpSource_i, "Cannot set socket SO_LINGER: " << strerror(errno));
        }

        // Attempt to connect
        LOG_DEBUG(RTLTcpSource_i, "Attemping connection...");
        if (connect(_rtl_socket, ip_src->ai_addr, ip_src->ai_addrlen)) {
            std::stringstream errmsg;
            errmsg << "Failed to open RTL connection: " << std::string(strerror(errno));
            LOG_ERROR(RTLTcpSource_i, errmsg.str());
            throw new CF::Resource::StartError(CF::CF_NOTSET, errmsg.str().c_str());
        }

        if (ai) {
            freeaddrinfo(ai);
            ai = 0;
        }

        int flag = 1;
        if (setsockopt(_rtl_socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(flag))) {
            LOG_WARN(RTLTcpSource_i, "Cannot set socket TCP_NODELAY");
        }

        int ret = recv(_rtl_socket, (char*) &_dongle_info, sizeof(_dongle_info), 0);
        if (sizeof(_dongle_info) != ret) {
            LOG_ERROR(RTLTcpSource_i, "Failed to read dongle info.  Read " << ret << " expected " << sizeof(_dongle_info));
            throw new CF::Resource::StartError(CF::CF_NOTSET, "Failed to read dongle info");
        }

        _tuner_type = RTLSDR_TUNER_UNKNOWN;
        _tuner_gain_count = 0;
        _tuner_if_gain_count = 0;

        if (memcmp(_dongle_info.magic, "RTL0", 4) != 0) {
            LOG_ERROR(RTLTcpSource_i, "Failed to find dongle magic number, did you connect to an RTL?");
            throw new CF::Resource::StartError(CF::CF_NOTSET, "Connection does not appear to be an RTL dongle");
        }

        _tuner_type = ntohl(_dongle_info.tuner_type);
        _tuner_gain_count = ntohl(_dongle_info.tuner_gain_count);
        if (RTLSDR_TUNER_E4000 == _tuner_type) {
            _tuner_if_gain_count = 53; // Magic number borrowed from gr-rtl
            if_gain.resize(6); // six gain stages
        } else {
            _tuner_if_gain_count = 0;
        }

        for (size_t i=0; i< if_gain.size(); i++) {
            if_gain[i] = 0;
        }

        LOG_DEBUG(RTLTcpSource_i, "Connected to tuner type: " << get_tuner_name(static_cast<enum rtlsdr_tuner>(_tuner_type)));
        LOG_DEBUG(RTLTcpSource_i, "Connected to tuner gain count: " << _tuner_gain_count);
        LOG_DEBUG(RTLTcpSource_i, "Connected to tuner if gain count: " << _tuner_if_gain_count);
    } catch (CF::Resource::StartError &ex) {
        if (ai)
            freeaddrinfo(ai);
        throw;
    } catch (...) {
        if (ai)
            freeaddrinfo(ai);
        LOG_ERROR(RTLTcpSource_i, "Unexpected error starting");
        throw new CF::Resource::StartError(CF::CF_NOTSET, "Unexpected error starting");
    }

    // Initialize the device to the desired properties
    onconfigure_rtl_property("*");

    initializeSRI();

    RTLTcpSource_base::start();
}

void RTLTcpSource_i::stop() throw (CF::Resource::StopError, CORBA::SystemException)
{
    boost::recursive_mutex::scoped_lock lock(_componentLock);

    if (!started()) {
        LOG_DEBUG(RTLTcpSource_i, "Device already stopped");
        return;
    }

    RTLTcpSource_base::stop(); // Stop the serviceThread

    if (_rtl_socket != -1) {
        shutdown(_rtl_socket, SHUT_RDWR);
        _rtl_socket = -1;
    }

    // Tell the down stream we are done after the serviceThread has stopped
    {
        boost::mutex::scoped_lock lock(serviceThreadLock);
        // Use static to avoid memory creation, clear just in case
        static std::vector<unsigned char> emptyVector; emptyVector.clear();
        static std::vector<short int> emptyVectorInt; emptyVectorInt.clear();
        static std::vector<float> emptyVectorFloat; emptyVectorFloat.clear();

        BULKIO::PrecisionUTCTime tstamp = _createTimestamp();
        ComplexIQ_uByte->pushPacket(emptyVector, tstamp, true, streamID);
        ComplexIQ_Short->pushPacket(emptyVectorInt, tstamp, true, streamID);
        ComplexIQ_Float->pushPacket(emptyVectorFloat, tstamp, true, streamID);
    }
}

int RTLTcpSource_i::serviceFunction()
{
    boost::recursive_mutex::scoped_lock lock(_componentLock); // Also grab the componentLock

    // TODO support reconnect and on the fly changes to rtl_host and rtl_port
    if (_rtl_socket == -1) {
        LOG_ERROR(RTLTcpSource_i, "Socket is no longer valid, stopping");
        try {
            stop();
        } catch (...) {
            LOG_ERROR(RTLTcpSource_i, "Failed to shutdown socket");
        }
        return NOOP;
    }

    // Allow output_block_size to change on the fly
    if (_outputData.size() != static_cast<size_t>(output_block_size)) {
        _outputData.resize(output_block_size * sizeof(unsigned char));
    }

    size_t index = 0;
    int receivedbytes;
    while (index < _outputData.size()) {
        int bytesleft = _outputData.size() - index;
        if (bytesleft <= 0) break; // should never occur

        receivedbytes = recv(_rtl_socket, (char*)&_outputData[index], bytesleft, 0);
        if ((receivedbytes == -1) && (errno != EAGAIN)) {
            LOG_ERROR(RTLTcpSource_i, "Socket recv has failed, stopping");
            try {
                stop();
            } catch (...) {
                LOG_ERROR(RTLTcpSource_i, "Failed to shutdown socket");
            }
            return NOOP;
        }

        index += receivedbytes;
    }

    static BULKIO::PrecisionUTCTime tstamp = _createTimestamp();

    if (_changedSRI) {
        _changedSRI = false;
        LOG_DEBUG(RTLTcpSource_i, "New SRI");
        printSRI(_sri);

        ComplexIQ_uByte->pushSRI(_sri);
        ComplexIQ_Short->pushSRI(_sri);
        ComplexIQ_Float->pushSRI(_sri);
    }

    ComplexIQ_uByte->pushPacket(_outputData, tstamp, false, streamID);
    // Only do conversions if someone is listening
    if (ComplexIQ_Short->connections()->length() > 0) {
        static std::vector<short int> realOut;
        realOut.resize(_outputData.size());
        for (size_t j = 0; j < _outputData.size(); j++) {
            realOut[j] = (short int)(static_cast<short int>(_outputData[j]) - 128) << 8;
        }
        ComplexIQ_Short->pushPacket(realOut, tstamp, false, streamID);
    }
    if (ComplexIQ_Float->connections()->length() > 0) {
        static std::vector<float> floatOut;
        floatOut.resize(_outputData.size());
        for (size_t j = 0; j < _outputData.size(); j++) {
            floatOut[j] = (float)(_outputData[j] - 127.5);
        }
        ComplexIQ_Float->pushPacket(floatOut, tstamp, false, streamID);
    }

    return NORMAL;
}

inline BULKIO::PrecisionUTCTime RTLTcpSource_i::_createTimestamp() {
    struct timeval tmp_time;
    struct timezone tmp_tz;
    gettimeofday(&tmp_time, &tmp_tz);
    double wsec = tmp_time.tv_sec;
    double fsec = tmp_time.tv_usec / 1e6;
    BULKIO::PrecisionUTCTime tstamp = BULKIO::PrecisionUTCTime();
    tstamp.tcmode = BULKIO::TCM_CPU;
    tstamp.tcstatus = (short)1;
    tstamp.toff = 0.0;
    tstamp.twsec = wsec;
    tstamp.tfsec = fsec;

    return tstamp;
}

void RTLTcpSource_i::initializeSRI() {
    boost::mutex::scoped_lock lock(serviceThreadLock);

    if (streamID == "") {
        srand(time(NULL));
        std::stringstream stream_id;
        stream_id << "rtl_tcp://" << rtl_host << "/" << rand();
        streamID = stream_id.str();
    }

    _sri.hversion = 1;
    _sri.xstart = 0.0;
    _sri.xdelta = 1.0 / (double)(sample_rate);
    _sri.xunits = BULKIO::UNITS_TIME;
    _sri.subsize = 0;
    _sri.ystart = 0.0;
    _sri.ydelta = 0.0;
    _sri.yunits = BULKIO::UNITS_NONE;
    _sri.mode = 1;
    _sri.streamID = streamID.c_str();
    _sri.blocking = false;
    CORBA::String_member col = "COL_RF";
    setKeywordByID<CORBA::Long>(_sri, col, (int)(frequency));

    _changedSRI = true;
}

void RTLTcpSource_i::updateSRI() {
    _sri.xdelta = 1.0 / (double)(sample_rate);
    CORBA::String_member col = "COL_RF";
    setKeywordByID<CORBA::Long>(_sri, col, (int)(frequency));
    _changedSRI = true;
}

void RTLTcpSource_i::printSRI(BULKIO::StreamSRI& sri) {
    std::stringstream ssri;
    ssri << std::endl;
    ssri << "SRI->hversion = " << _sri.hversion << std::endl;
    ssri << "SRI->xstart = "   << _sri.xstart   << std::endl;
    ssri << "SRI->xdelta = "   << _sri.xdelta   << std::endl;
    ssri << "SRI->xunits = "   << _sri.xunits   << std::endl;
    ssri << "SRI->subsize = "  << _sri.subsize  << std::endl;
    ssri << "SRI->ystart = "   << _sri.ystart   << std::endl;
    ssri << "SRI->ydelta = "   << _sri.ydelta   << std::endl;
    ssri << "SRI->yunits = "   << _sri.yunits   << std::endl;
    ssri << "SRI->mode = "     << _sri.mode     << std::endl;
    ssri << "SRI->streamID = " << _sri.streamID << std::endl;
    ssri << "SRI->blocking = " << _sri.blocking << std::endl;
    LOG_DEBUG(RTLTcpSource_i, ssri.str());
}
