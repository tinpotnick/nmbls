
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

#include <unordered_map>

#include "httpconnection.h"
#include "config.h"

#include "nmbls.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

namespace nmbls
{
  /* our server */
  boost::asio::io_service io_service;

  /*******************************************************************************
  Class: nmbls
  Description: Listen for new connections and spawn new object to handle.
  Date: 15.11.2017
  Author: Nick Knight
  *******************************************************************************/
  class nmbls
  {
  public:
  	nmbls(boost::asio::io_service& io_service)
  		: acceptor_(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), HTTPLISTENPORT))
  	{
  		start_accept();
  	}

  private:
  	void start_accept()
  	{
  		httpconnection::pointer new_connection =
  			httpconnection::create(acceptor_.get_io_service());

  		acceptor_.async_accept(new_connection->socket(),
  			boost::bind(&nmbls::handle_accept, this, new_connection,
  			boost::asio::placeholders::error));
  	}

  	void handle_accept(httpconnection::pointer new_connection,
  		const boost::system::error_code& error)
  	{
  		if (!error)
  		{
  			new_connection->start();
  			start_accept();
  		}
  	}

  	boost::asio::ip::tcp::acceptor acceptor_;
  };

  typedef std::unordered_map< std::string, nmblshandler *> handlersdef;
  static handlersdef handlers;

  /*******************************************************************************
  Function: addhandler
  Description: Adds a web handler to our server
  Date: 21.11.2017
  Author: Nick Knight
  *******************************************************************************/
  void addhandler( nmblshandler *handler )
  {
    handlers[ handler->uri ] = handler;
  }

  /*******************************************************************************
  Function: gethandler
  Description: Returns a handler for a uri
  Date: 05.12.2017
  Author: Nick Knight
  *******************************************************************************/
  nmblshandler *gethandler( std::string uri )
  {
    handlersdef::iterator it;
    it = handlers.find( uri );

    if( handlers.end() == it )
    {
      return NULL;
    }
    return it->second;
  }

  /*******************************************************************************
  Function: daemonize
  Description: Daemonize the program.
  Date: 15.11.2017
  Author: Nick Knight
  *******************************************************************************/
  void damonizeserver( void )
  {
      pid_t pid, sid;

      /* already a daemon */
      if ( getppid() == 1 ) return;

      struct rlimit core_limits;
      core_limits.rlim_cur = core_limits.rlim_max = RLIM_INFINITY;
      setrlimit(RLIMIT_CORE, &core_limits);

      /* Fork off the parent process */
      pid = fork();
      if (pid < 0)
      {
          exit(EXIT_FAILURE);
      }
      /* If we got a good PID, then we can exit the parent process. */
      if (pid > 0)
      {
          exit(EXIT_SUCCESS);
      }

      /* At this point we are executing as the child process */

      /* Change the file mode mask */
      umask(0);

      /* Create a new SID for the child process */
      sid = setsid();
      if (sid < 0)
      {
          exit(EXIT_FAILURE);
      }

      /* Change the current working directory.  This prevents the current
         directory from being locked; hence not being able to remove it. */
      if ((chdir("/")) < 0)
      {
          exit(EXIT_FAILURE);
      }

      /* Redirect standard files to /dev/null */
      freopen( "/dev/null", "r", stdin);
      freopen( "/dev/null", "w", stdout);
      freopen( "/dev/null", "w", stderr);
  }

  /*******************************************************************************
  Fucntion: StopServer
  Description: As it says.
  Date: 15.11.2017
  Author: Nick Knight
  *******************************************************************************/
  void stopserver( void )
  {
  	io_service.stop();
  }

  /*******************************************************************************
  Function: killserver
  Description: Our ctrl-c handler.
  Date: 15.11.2017
  Author: Nick Knight
  *******************************************************************************/
  static void killserver( int signum )
  {
    std::cout << "OUCH" << std::endl;
    stopserver();
  }

  /*******************************************************************************
  Fucntion: StartServer
  Description: As it says. Checks for the --debug flag on the command line to
  see if it should damonize the sevrer or not.
  Date: 15.11.2017
  Author: Nick Knight
  *******************************************************************************/
  void startserer( int argc, const char* argv[] )
  {
    bool debug = false;

    for ( int i = 1; i < argc ; i++ )
    {
      if ( argv[i] != NULL )
      {
        std::string argvstr = argv[i];

        if ( "--debug" == argvstr)
        {
          debug = true;
        }
      }
    }
    if ( !debug )
    {
      damonizeserver();
      return;
    }

    /* Register our CTRL-C handler */
    signal( SIGINT, killserver );

  	try
  	{
  		nmbls server(io_service);
  		io_service.run();
  	}
  	catch (std::exception& e)
  	{
  		std::cerr << e.what() << std::endl;
  	}

  	/* Clean up */
  	std::cout << "Cleaning up" << std::endl;
  	return;
  }
}
