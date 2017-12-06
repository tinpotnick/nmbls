/*
 * httpconnection.h
 *
 *  Created on: May 10, 2013
 *      Author: nick
 */

#ifndef HTTP_CONNECTION_H_
#define HTTP_CONNECTION_H_

#include <boost/enable_shared_from_this.hpp>
#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>

#include <boost/unordered_map.hpp>

#include <list>
#include <deque>
#include <string>

#include <exception>

#include "httpdoc.h"
#include "config.h"

namespace nmbls
{

  typedef std::deque<std::string> stringqueue;

  typedef std::vector<char> charvector;
  typedef std::deque<charvector> charvectorqueue;

  ///////////////////////////////////////////////////////////////
  //
  // Class httpconnection
  //
  // Represents a connection to a client using http. one is created
  // for every connection made.
  //
  ///////////////////////////////////////////////////////////////
  class httpconnection
  	: public boost::enable_shared_from_this<httpconnection>
  {
  public:
  	typedef boost::shared_ptr<httpconnection> pointer;

  	static pointer create(boost::asio::io_service& io_service);
  	boost::asio::ip::tcp::socket& socket();
  	void start();

  	httpconnection(boost::asio::io_service& io_service);
  	~httpconnection();

  	boost::asio::ip::tcp::socket socket_;
  	boost::asio::deadline_timer timer_;
  	boost::asio::io_service &io_service_;
  	/// Buffer for incoming data.
  	charvector readbuffer;

  	void postwstextframe(std::string txt);
  	void postdocument(std::string fullhttpresponce);

  	void postunauthorized( std::string message = "Unauthorised", std::string realm = "" );
  	void postnotfound(std::string message = "<html><body><b>Page not found, please correct the url and try again.</b></body></html>");

  	std::string CalcSha1(std::string input);

  private:

  	void handleread(const boost::system::error_code& error, std::size_t bytes_transferred);
  	void handlewrite(const boost::system::error_code& error, std::size_t bytes_transferred);
  	void handlewswrite(const boost::system::error_code& error, std::size_t bytes_transferred);

  	void handlerequest( httpdoc &ourdoc );
  	void postpong( void );
  	void postping( void );
  	bool handlewsupgrade( httpdoc &ourdoc );
  	void handlewspacket( void );

  	bool KeepAlive;
  	bool responceWaiting;

  	/* Normal http document text queue */
  	stringqueue webqueue;

  	/* When switched to websocket */
  	charvectorqueue websocketqueue;

  	enum httpconnectiontypes { http_http, http_websocket };
  	httpconnectiontypes httpConnType;

  	int websocketoutogingpingcount;

  };


  class httpexception : public std::exception
  {
  public:
    std::string code;
    std::string message;

    httpexception( std::string code = "500", std::string message = "" ) { this->code = code; this->message = message; }
  };

}

#endif /* HTTP_CONNECTION_H_ */
