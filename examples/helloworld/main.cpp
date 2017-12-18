
#include <stdio.h>
#include <nmbls/nmbls.h>

#include <lua.hpp>

/*******************************************************************************
Class: roothandler
Description: Most basic of Hello World handler.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
class roothandler : public nmbls::nmblshandler
{
public:
  roothandler()
  {
    this->uri = "";
  }

  virtual void onhttp( httpdoc &in, httpdoc &out )
  {
    out.body << "Hello World";
  }
};

/*******************************************************************************
Class: templatehandler
Description: Example handler which uses simpletemplate.
Date: 17.12.2017
Author: Nick Knight
*******************************************************************************/
class templatehandler : public nmbls::nmblshandler
{
private:
  nmbls::simpletemplate outputtemplate;
public:
  templatehandler()
  {
    this->uri = "template";

    /*
      simpletemplate efficiently compiles the template so that this is done once
      on load. Then when required a boost::ptree is created to populate the variables.

      A variable or keyword can be included in {{ }}, so for example, {{ weather }}
      or we can create a for loop:
      {{ for i = 1:10 }}
      It can also take an increment amount, so:
      {{ for i = 1:2:10 }}
      It should be closed with {{ end }}
      We can also create an if statement:
      {{ if weather == sunny }}

      {{ end }}
      If there is a variable by that name it will substitue that string (weather),
      if not it will take it as a string literal (sunny).
    */
    outputtemplate.compile( R"d(
{{ message }}.
Thank you for visiting my web page.
Today's weather is {{ weather }}.
{{ if weather == sunny }}
I like days like to today.
I can also count to 10: {{ for i = 1:10 }} {{ i }} {{ end }}
{{ end }}
)d" );

  }

  virtual void onhttp( httpdoc &in, httpdoc &out )
  {
    std::string weather = in.getqueryvalue( "weather" );
    boost::property_tree::ptree tree;
    tree.put( "message", "Hello World" );
    tree.put( "weather", weather );

    out.body << outputtemplate.render( tree );
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
  nmbls::addhandler( new templatehandler() );
  nmbls::startserver( argc, argv );
	return 0;
}
