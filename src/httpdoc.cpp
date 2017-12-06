/*
 * httpdoc.cpp
 *
 * Created on: November 15th 2017
 * Author: Nick Knight
 */


#include <algorithm>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <algorithm>
#include "httpdoc.h"

/*******************************************************************************
Function: Constructor
Description: parses a string which represents a HTTP get request and
stores all of the information is usable variables.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
httpdoc::httpdoc(std::string docstr)
{
	this->parse( docstr );
}


/*******************************************************************************
Function: httpdoc::parse
Description: parses a string which represents a HTTP get request and
stores all of the information is usable variables.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void httpdoc::parse(std::string docstr)
{
	/* pull out our headers */
	this->parseheaders(docstr);
	/* retain it just in case */
	std::size_t pos = docstr.find("GET");
  this->httpdoctype = httpdoc_get;
  if ( std::string::npos == pos )
  {
    this->httpdoctype = httpdoc_post;
    pos = docstr.find("POST");
    if ( std::string::npos == pos )
    {
      this->httpdoctype = httpdoc_put;
      pos = docstr.find("PUT");
      if ( std::string::npos == pos )
      {
        this->httpdoctype = httpdoc_delete;
        pos = docstr.find("DELETE");
        if ( std::string::npos == pos )
        {
          return;
        }
      }
    }
  }

	std::size_t endpos = docstr.find("HTTP", pos);
	if ( std::string::npos == endpos )
	{
		return;
	}

	std::size_t paramspos = docstr.find('?', pos);
	if ( std::string::npos != paramspos )
	{
		this->query = docstr.substr(paramspos,endpos - paramspos-1);
	}

	if ( std::string::npos == paramspos )
	{
		this->requesturi = docstr.substr(pos+4, endpos-pos-5);
	}
	else
	{
		this->requesturi = docstr.substr(pos+4, paramspos - (pos+4));
	}

	if ( '/' == this->requesturi[0] )
	{
		this->requesturi.erase(0,1);
	}

	if ( std::string::npos != paramspos )
	{

		std::string params = docstr.substr(paramspos+1, endpos-paramspos-2);
		StringVector strs;
		boost::split( strs, params, boost::is_any_of( "&" ) );

		for ( StringVector::iterator it = strs.begin(); it != strs.end(); it++ )
		{
			StringVector parts;
			boost::split( parts, *it, boost::is_any_of( "=" ) );
			switch ( parts.size() )
			{
				case 1:
				{
					this->queryparams[ parts[ 0 ] ] =  "";
					break;
				}
				case 2:
				{
					this->queryparams[ parts[ 0 ] ] =  parts[ 1 ];
					break;
				}
				default:
				{
					// This shouldn't happen
				}
			}
		}
	}
}

/*******************************************************************************
Function: HttpDoc::parseheaders
Description: parses the docs headers.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void httpdoc::parseheaders(std::string docstr)
{
	std::size_t endofheaders = docstr.find("\r\n\r\n");

	if ( std::string::npos == endofheaders)
	{
		return;
	}

	std::string headers = docstr.substr(0, endofheaders+2);
	std::size_t endofline;

	while ( std::string::npos != (endofline = headers.find("\r\n") ) )
	{
		std::string line = headers.substr(0, endofline);
		std::size_t parambreak;
		if ( std::string::npos == ( parambreak = line.find(":") ) )
		{
			headers = headers.substr(endofline + 2 , headers.size());
			continue;
		}

		std::string param = line.substr(0, parambreak);
		std::string value = line.substr(parambreak + 1, line.size());

		TrimRight(value, " ");
		TrimLeft(value, " ");

		std::transform( param.begin(),param.end(),param.begin(),tolower );

		std::cout << "Inserting this->httpheaders[" << param << "]=" << value << std::endl;
		this->httpheaders[ param ] = value;

		headers = headers.substr( endofline + 2 , headers.size() );
	}
}

/*******************************************************************************
Function: httpdoc::getqueryvalue
Description: Returns a variable from a query string.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
std::string httpdoc::getqueryvalue(std::string name)
{
	return urldecode( this->queryparams[ name ] );
}

/*******************************************************************************
Function: httpdoc::geturi
Description: Returns the requested filename.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
std::string httpdoc::geturi( void )
{
	return urldecode( this->requesturi );
}

/*******************************************************************************
Function: httpdoc::getquerystring
Description: Returns the query string - everything after a ?
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
std::string httpdoc::getquerystring( void )
{
	return this->query;
}

/*******************************************************************************
Function: httpdoc::getheader
Description: Returns a HTTP Header, the name MUST be in lower case as this is
how they are stored to make them case insensative according to RFC 2616.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
std::string httpdoc::getheader(std::string header)
{
	std::cout << "Retrieving this->httpheaders[" << header << "]=" << this->httpheaders[ header ] << std::endl;
	return this->httpheaders[ header ];
}

/*******************************************************************************
Function: httpdoc::addheader
Description: Adds a HTTP header to the document. Each header field consists of a
name followed by a colon (":") and the field value. Field names are
case-insensitive. Values are not.
Date: 30.11.2017
Author: Nick Knight
*******************************************************************************/
void httpdoc::addheader( std::string header, std::string value )
{
	std::transform( header.begin(), header.end(), header.begin(), tolower );
  this->httpheaders[ header ] = value;
}

/*******************************************************************************
Function: httpdoc::generatehttpdoc
Description: Generates a document body which can be sent back to the client.
Date: 05.12.2017
Author: Nick Knight
*******************************************************************************/
std::string httpdoc::generatehttpdoc( std::string errcode, std::string message )
{
  std::string doc( "HTTP/1.1 " + errcode + " " + message + std::string("\r\n") );
  this->addheader( "content-length", std::to_string( this->body.str().size() ) );

  // Now add the headers.
  stringmap::iterator it;
  for ( it = this->httpheaders.begin(); it != this->httpheaders.end(); it++ )
  {
    doc += it->first + std::string( ":" ) + it->second + std::string( "\r\n" );
  }

  if ( this->body.str().size() > 0 )
  {
    doc += "\r\n";
    doc += this->body.str();
  }
  else
  {
    doc += "\r\n";
  }


  return doc;
}
