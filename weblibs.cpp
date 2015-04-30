/*
 * weblibs.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: Nick Knight
 *
 * See LICENCE for more details.
 */

#include <iostream>

#include "weblibs.h"

extern bool is_running_as_debug;
extern boost::asio::io_service io_service;

libraries ourloadedlibs;

weblib_ptr loadlib(std::string file, std::string root)
{
	libraries::iterator it = ourloadedlibs.find(file);
	if ( it != ourloadedlibs.end() )
	{
		// already loaded
		return it->second;
	}

	weblib_ptr ptr(new weblib(root, file));
	ourloadedlibs[file] = ptr;
	return ptr;

}

weblib::weblib(std::string root, std::string lib)
{
	if (is_running_as_debug) std::cout << "Loading library " << lib << std::endl;

	this->lib = lib;
	this->library = dlopen(this->lib.c_str(), RTLD_LAZY );
	if (!this->library)
	{
		throw std::invalid_argument(std::string("Failed to load library: ") + lib + std::string(" with the error ") + dlerror());
	}
	if (is_running_as_debug) std::cout << "Loaded library " << lib << std::endl;

	this->loadLibrary = (handleLoadLibraryFunc) dlsym(this->library, "handleLoadLibrary");
	if (!this->loadLibrary) throw std::invalid_argument(std::string("No valid handleLoadLibrary function available in ") + lib);
	this->NewConnection = (handleNewWSConnectionFunc) dlsym(this->library, "handleNewWSConnection");
	if (!this->NewConnection) throw std::invalid_argument(std::string("No valid handleNewWSConnection function available in ") + lib);
	this->DestroyConnection = (handleDestroyConnectionFunc) dlsym(this->library, "handleDestroyConnection");
	if (!this->DestroyConnection) throw std::invalid_argument(std::string("No valid handleDestroyConnection function available in ") + lib);
	this->TextPacket = (handleTextPacketFunc) dlsym(this->library, "handleTextPacket");
	if (!this->TextPacket) throw std::invalid_argument(std::string("No valid handleTextPacket function available in ") + lib);
	this->WebRequest = (handleWebRequestFunc) dlsym(this->library, "handleWebRequest");
	if (!this->WebRequest) throw std::invalid_argument(std::string("No valid handleWebRequest function available in ") + lib);
	this->handleUnLoadLibrary = (handleUnLoadLibraryFunc) dlsym(this->library, "handleUnLoadLibrary");
	if (!this->handleUnLoadLibrary) throw std::invalid_argument(std::string("No valid handleUnLoadLibrary function available in ") + lib);

	this->loadLibrary(io_service, root, is_running_as_debug);
}

weblib::~weblib()
{
	if (is_running_as_debug) std::cout << "Cleaning up library " << this->lib << std::endl;
	this->handleUnLoadLibrary();
	dlclose(this->library);
}
