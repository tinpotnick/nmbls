/*
 * nmbls.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: Nick Knight
 *
 * See LICENCE for more details.
 */


#include <iostream>


#include <stdio.h>
#include <signal.h>

#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <boost/asio.hpp>

#include <sys/resource.h>

// For JSON support
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>

#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>

#include "weblibs.h"
#include "http_connection.h"
#include "nmbls.h"

#include "http_server.h"
#include "https_server.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define HTTPLISTENPORT 80
#define HTTPSLISTENPORT 443

bool is_running_as_debug;

/* our server */
boost::asio::io_service io_service;
boost::thread::id mainthreadid;
std::string serverroot;

static void daemonize(void)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}

///////////////////////////////////////////////////////////////
//
// Function StopServer
//
// As it says
//
///////////////////////////////////////////////////////////////
static void StopServer(void)
{
	io_service.stop();
}

static void createDefaultConfig(std::string root)
{
    std::cout << "Creating default config" << std::endl;
    boost::filesystem::create_directory(root);

    boost::property_tree::ptree pt;
    boost::property_tree::ptree lib, libs;

    lib.put("", "libsqlite.so");
    libs.push_back(std::make_pair("", lib));

    pt.add_child("libs", libs);

    std::string dir = root + "/";
    std::string config = dir + "/nmbls.json";

    write_json(config, pt);
}

void loadConfig(std::string root)
{
    std::string dir = root + "/";
    std::string config = dir + "/nmbls.json";

    if ( !boost::filesystem::is_regular_file(config) )
    {
        createDefaultConfig(root);
    }

    try
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(config, pt);

        boost::property_tree::ptree::const_iterator it;
        boost::property_tree::ptree libs = pt.get_child("libs");
        boost::property_tree::ptree::const_iterator libsit;

        for (libsit = libs.begin(); libsit != libs.end(); ++libsit)
        {
            std::string lib = libsit->second.get<std::string>("");
            std::cout << "Need lib " << lib << std::endl;

            loadlib(lib, root);
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
    }
}


/* our server */
void StartServer(std::string root)
{

	// This section will need to be done via config..
	//loadvserver("127.0.0.1", "./libwebmem.so");
	//loadvserver("127.0.0.1", "./libwebqueue.so");

    serverroot = root;
    loadConfig(root);
    loadMimeTypes();

	std::cout << "Starting nmbls" << std::endl;
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), HTTPLISTENPORT);

	try
	{
                http_server server(io_service, endpoint);
                try
                {
                    // If we can't load certificate, then we will just let know and not start then
                    // HTTPS service.
                    https_server servers(io_service, HTTPSLISTENPORT);
                }
                catch(std::exception& e)
                {
                    std::cerr << e.what() << std::endl;
                }
                mainthreadid = boost::this_thread::get_id();
                io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	// Clean up
	std::cout << "Cleaning up" << std::endl;
	return;
}

static void killServer(int signum)
{
	printf("OUCH\n");
	StopServer();
}

int main(int argc, const char* argv[])
{
    std::string root;
	srand(time(NULL));

	is_running_as_debug = false;

	for ( int i = 1; i < argc ; i++ )
		{
			if ( argv[i] != NULL )
			{
				std::string argvstr = argv[i];

				if ( "--root" == argvstr)
				{
                                    if ( argc > (i+1))
                                    {
                                        root = argv[i+1];
                                        i++;
                                    }
				}

				else if ( "--debug" == argvstr)
				{
					is_running_as_debug = true;
				}
			}
		}

        if ( "" == root )
        {
            std::cerr << "You need to supply the --root argument" << std::endl;
            return 0;
        }

	// Register our CTRL-C handler
	signal(SIGINT, killServer);

	struct rlimit core_limits;
	core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limits);

	if ( !is_running_as_debug )
	{
		daemonize();
	}

	StartServer(root);

	return 0;
}
