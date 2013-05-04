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
#ifndef RTLTCPSOURCE_IMPL_H
#define RTLTCPSOURCE_IMPL_H

#include "RTLTcpSource_base.h"

// For socket stuff
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>

class RTLTcpSource_i;

// REDHAWK Implementation of a component can can speak to an rtl_tcp instance
//

//////////////////////////////////////////////////////////////////////////////
// copied from rtl sdr code
typedef struct
{ /* structure size must be multiple of 2 bytes */
    char magic[4];
    uint32_t tuner_type;
    uint32_t tuner_gain_count;
} dongle_info_t;

enum rtlsdr_tuner
{
    RTLSDR_TUNER_UNKNOWN = 0,
    RTLSDR_TUNER_E4000,
    RTLSDR_TUNER_FC0012,
    RTLSDR_TUNER_FC0013,
    RTLSDR_TUNER_FC2580,
    RTLSDR_TUNER_R820T
};

const int e4k_gains[] = { -10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420 };
const int fc0012_gains[] = { -99, -40, 71, 179, 192 };
const int fc0013_gains[] = { -99, -73, -65, -63, -60, -58, -54, 58, 61, 63, 65, 67, 68, 70, 71, 179, 181, 182, 184,
        186, 188, 191, 197 };
const int fc2580_gains[] = { 0 /* no gain values */};
const int r820t_gains[] = { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157, 166, 197, 207, 229, 254, 280, 297, 328, 338, 364,
        372, 386, 402, 421, 434, 439, 445, 480, 496 };

const int e4k_if_gains[] = { -3, 240, 290, 340, 420 };

struct command
{
    unsigned char cmd;
    unsigned int param;
}__attribute__((packed));

static inline std::string get_tuner_name( enum rtlsdr_tuner tuner_type )
{
  if ( RTLSDR_TUNER_E4000 == tuner_type )
    return "E4000";
  else if ( RTLSDR_TUNER_FC0012 == tuner_type )
    return "FC0012";
  else if ( RTLSDR_TUNER_FC0013 == tuner_type )
    return "FC0013";
  else if ( RTLSDR_TUNER_FC2580 == tuner_type )
    return "FC2580";
  else if ( RTLSDR_TUNER_R820T == tuner_type )
    return "R820T";
  else
    return "Unknown";
}

// End of copied code
//////////////////////////////////////////////////////////////////////////////

class RTLTcpSource_i: public RTLTcpSource_base
{
ENABLE_LOGGING
public:
    RTLTcpSource_i(const char *uuid, const char *label);
    ~RTLTcpSource_i();
    int serviceFunction();

    // Extending base class
    void initialize() throw (CF::LifeCycle::InitializeError, CORBA::SystemException);
    void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);
    void start() throw (CF::Resource::StartError, CORBA::SystemException);
    void stop() throw (CF::Resource::StopError, CORBA::SystemException);
    void configure(const CF::Properties& configProperties)
            throw (CF::PropertySet::PartialConfiguration, CF::PropertySet::InvalidConfiguration,
            CORBA::SystemException);
protected:
    inline BULKIO::PrecisionUTCTime _createTimestamp();

    void onconfigure_rtl_property(const std::string& id);

    void validate(CF::Properties property, CF::Properties& validProps, CF::Properties& invalidProps);

    void initializeSRI();
    void updateSRI();
    void printSRI(BULKIO::StreamSRI& sri);

    // Function to set an SRI keyword value
    template <typename TYPE> bool setKeywordByID(BULKIO::StreamSRI &sri, CORBA::String_member id, TYPE value) {
        /****************************************************************************************************************
         * Description: Set a value to an existing keyword, or add a new keyword/value pair if it doesn't already exist.
         * sri   - StreamSRI object to process
         * id    - Keyword identifier string
         * value - The value to set.
         */
        CORBA::Any corbaValue;
        corbaValue <<= value;
        unsigned long keySize = sri.keywords.length();

        // If keyword is found, set it and return true
        for(unsigned long i=0; i < keySize; i++) {
            if(!strcmp(sri.keywords[i].id, id)) {
                sri.keywords[i].value = corbaValue;
                return true;
            }
        }

        // Otherwise, add keyword and set it before returning true
        sri.keywords.length(keySize+1);
        if(sri.keywords.length() != keySize+1) // Ensure the length has been adjusted
        {
            std::cout<<"RIGHT HERE --- "<<std::endl;
            return false;
        }
        sri.keywords[keySize].id = CORBA::string_dup(id);
        sri.keywords[keySize].value = corbaValue;
        return true;
    }

private:
    // Since the serviceThreadLock is *NOT* reentrant, use
    // or own lock to avoid race-conditions
    boost::recursive_mutex _componentLock;

    bool _changedSRI;
    int _rtl_socket;

    // Information about the dongle
    dongle_info_t _dongle_info;
    unsigned int _tuner_type;
    unsigned int _tuner_gain_count;
    unsigned int _tuner_if_gain_count;

    // Output buffer
    BULKIO::StreamSRI _sri;
    std::vector<unsigned char> _outputData;
};

#endif
