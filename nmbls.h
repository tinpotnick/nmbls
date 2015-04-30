/*
 * nmbls.h
 *
 *  Created on: Jan 13, 2015
 *      Author: nick
 *
 *  This is our public interface
 */

#ifndef NMBLSMAINHEADER_H_
#define NMBLSMAINHEADER_H_

#define NMBLSEXPORT extern "C" __attribute__((visibility("default")))

#include <string>
#include <boost/asio.hpp>
#include <boost/unordered_map.hpp>

typedef void * websockcon;
//typedef std::map<std::string, std::string> ValuePair;
typedef boost::unordered_map<std::string, std::string> ValuePair;

/* We need to be fast - deine a way of testing us */
inline void measureuSTime(boost::posix_time::ptime &reference, bool is_running_as_debug, std::string message = "")
{
    if ( is_running_as_debug )
    {
        if ( "" != message )
        {
            boost::posix_time::time_duration diff = boost::posix_time::microsec_clock::local_time() - reference;
            std::cout << message << ": " << diff.total_microseconds() << "uS" << std::endl;
        }
        reference = boost::posix_time::microsec_clock::local_time();
    }
}

#endif /* NMBLSMAINHEADER_H_ */
