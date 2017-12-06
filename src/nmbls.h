
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
  void startserer( int argc, const char* argv[] );
  void stopserver( void );
  void damonizeserver( void );

  class nmblshandler
  {
  public:
    nmblshandler() {}
    ~nmblshandler() {}

    virtual void on( httpdoc &in, httpdoc &out ) { std::cout << "Unhandled GET request" << std::endl; };
    virtual void on( httpdoc &in, httpconnection &connection ) { std::cout << "Unhandled WS Upgrade" << std::endl; };
    virtual void on( httpconnection &connection ) { std::cout << "Unhandled WS Close" << std::endl; };
    virtual void on( websocketframe &frame, httpconnection &connection ) { std::cout << "Unhandled WS Frame" << std::endl; };

    std::string uri;
  };

  void addhandler( nmblshandler *handler );
  nmblshandler *gethandler( std::string uri );
}
