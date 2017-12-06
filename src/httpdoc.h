/*
 * httpdoc.h
 *
 *  Created on: May 10, 2013
 *      Author: nick
 */

#ifndef HTTP_DOC_H_
#define HTTP_DOC_H_

#include <sstream>
#include <iostream>
#include <boost/array.hpp>
#include <string>
#include <map>
#include <vector>
#include <list>

#include "config.h"

typedef boost::array<char, MAXPAYLOADSIZE> rawDataArray;

typedef std::map<std::string, std::string> stringmap;
typedef std::vector<std::string> StringVector;

/*******************************************************************************
Function: urldecode
Description: Looks for the %20 type characters and replaces them with the
ASCII equivalent (i.e. a %20 becomes a space).
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
inline std::string urldecode(std::string str)
{
	std::string retVal;

	for ( std::string::iterator it = str.begin() ; it != str.end() ; it ++ )
	{
		if ( '%' == (*it) )
		{
			std::string nibble;

			// Take the next two chars
			it++;
			nibble += (*it);
			it++;
			nibble += (*it);

			unsigned char i;
			int b;

			std::istringstream iss(nibble);
			// Some wierd thing I can't figure out!
			// if i go direct to a char - it doesn't work - it only takes half of the nibble.
			// if I go to an int first it works - it calculates it accross the whole of the nibble.
			iss >> std::hex >> b;
			i = b;

			retVal += i;
		}
		else if ( '+' == (*it) )
		{
			retVal += ' ';
		}
		else
		{
			retVal += (*it);
		}
	}
	return retVal;
}

/*******************************************************************************
Function: TrimLeft
Description: Trims whitespace on the left of a string.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
inline void TrimLeft(std::string& str, const char* chars2remove)
{
	if (!str.empty())
	{
		std::string::size_type pos = str.find_first_not_of(chars2remove);

		if (pos != std::string::npos)
			str.erase(0,pos);
		else
			str.erase( str.begin() , str.end() ); // make empty
	}
}

/*******************************************************************************
Function: TrimRight
Description: Trims whitespace on the right of a string.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
inline void TrimRight(std::string& str, const char* chars2remove)
{
	if (!str.empty())
	{
		std::string::size_type pos = str.find_last_not_of(chars2remove);

		if (pos != std::string::npos)
			str.erase(pos+1);
		else
			str.erase( str.begin() , str.end() ); // make empty
	}
}

/*******************************************************************************
Class: HttpDoc
Description: Parses a string which represents a HTTP get request and
stores all of the information is usable variables.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
class httpdoc
{
private:
	stringmap httpheaders;
	std::string query;
	std::string requesturi;
	stringmap queryparams;

public:
  
  std::stringstream body;
  enum httpdoctypes { httpdoc_get, httpdoc_post, httpdoc_put, httpdoc_delete };

	httpdoc( std::string docstr );
	httpdoc(){};

	void parseheaders( std::string getstr );
	void parse( std::string docstr );
	std::string getqueryvalue( std::string name );
	std::string getquerystring( void );
	std::string geturi( void );
	std::string getheader( std::string header );
	inline std::string getbody( void ) { return this->body.str(); }

	void addheader( std::string header, std::string value );
	inline void setbody( std::string body ) { this->body << body; };

	std::string generatehttpdoc( std::string errcode = "200", std::string message = "" );
	inline stringmap getqueryvariables( void ) { return this->queryparams; }

private:
	httpdoctypes httpdoctype;
};


#endif //HTTPDOC_H
