
#include <stdio.h>
#include <nmbls/nmbls.h>

class roothandler : public nmbls::nmblshandler
{
public:
  roothandler() : nmbls::nmblshandler()
  {
    this->uri = "";
  }

  virtual void onhttp( httpdoc &in, httpdoc &out )
  {
    out.body << "Hello World";
  }
};

/*******************************************************************************
Function: main
Description: Start our server.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
int main( int argc, const char* argv[] )
{
  nmbls::addhandler( new roothandler() );
  nmbls::startserver( argc, argv );
	return 0;
}
