
#include <iostream>
#include <stdio.h>
#include <signal.h>

#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "httpdoc.h"
#include "httpconnection.h"
#include "websocketframe.h"

namespace nmbls
{
  void startserver( int argc, const char* argv[] );
  void stopserver( void );
  void damonizeserver( void );

  class nmblshandler
  {
  public:
    nmblshandler() {}
    ~nmblshandler() {}

    virtual void onhttp( httpdoc &in, httpdoc &out ) { std::cout << "Unhandled HTTP request" << std::endl; };
    virtual void onwsupgrade( httpdoc &in, httpconnection &connection ) { std::cout << "Unhandled WS Upgrade" << std::endl; };
    virtual void onwsclose( httpconnection &connection ) { std::cout << "Unhandled WS Close" << std::endl; };
    virtual void onwsframe( websocketframe &frame, httpconnection &connection ) { std::cout << "Unhandled WS Frame" << std::endl; };

    std::string uri;
  };

  void addhandler( nmblshandler *handler );
  nmblshandler *gethandler( std::string uri );
}
