/*
 * virtualserver.h
 *
 *  Created on: Jan 29, 2015
 *      Author: nick
 *
 *  Functions and classes to run virtual servers against libraries
 */

#ifndef _WEBLIBS_H_
#define _WEBLIBS_H_

#include <dlfcn.h>
#include <map>
#include <stdexcept>
#include <boost/unordered_map.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

#include "nmbls.h"

extern "C" typedef void (*handleLoadLibraryFunc)(boost::asio::io_service &, std::string root, bool debug);
extern "C" typedef bool (*handleNewWSConnectionFunc)(websockcon ref,
                                                                                                std::string url,
                                                                                                ValuePair &httpheaders,
                                                                                                ValuePair &queryparams,
                                                                                                boost::function<void(std::string)> postTextCallback);

extern "C" typedef void (*handleDestroyConnectionFunc)(websockcon ref);
extern "C" typedef bool (*handleTextPacketFunc)(websockcon ref, std::string str);
extern "C" typedef bool (*handleWebRequestFunc)(std::string method, websockcon ref,
                                                                                                std::string file,
                                                                                                ValuePair &httpheaders,
                                                                                                ValuePair &queryparams,
                                                                                                boost::function<void(std::string, std::string, std::string)> postResponceCallback);
extern "C" typedef void (*handleUnLoadLibraryFunc)(void);


class weblib
{
public:
	weblib(std::string root, std::string lib);

	~weblib();
	handleNewWSConnectionFunc NewConnection;
	handleDestroyConnectionFunc DestroyConnection;
	handleTextPacketFunc TextPacket;
	handleWebRequestFunc WebRequest;

private:
	handleLoadLibraryFunc loadLibrary;
	handleUnLoadLibraryFunc handleUnLoadLibrary;
	void *library;
	std::string lib;
};

typedef boost::shared_ptr<weblib> weblib_ptr;
typedef std::map<std::string, weblib_ptr> libraries;


weblib_ptr loadlib(std::string file, std::string root);

#endif /* _WEBLIBS_H_ */
