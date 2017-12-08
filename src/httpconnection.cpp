/*
 * httpconnection.cpp
 *
 *  Created on: May 10, 2013
 *      Author: nick
 */

#include <sstream>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>

#include <boost/uuid/sha1.hpp>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <iomanip>

#include "nmbls.h"
#include "httpconnection.h"
#include "websocketframe.h"
#include "base64.h"

uint64_t connection_count = 0;
uint64_t current_connection_count = 0;

/*******************************************************************************
Class: httpconnection
Description: Listen for new connections and spawn new object to handle.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
nmbls::httpconnection::httpconnection(boost::asio::io_service& io_service)
                : socket_(io_service),
                timer_(io_service),
                io_service_(io_service),
                readbuffer( MAXPAYLOADSIZE )
{
  this->KeepAlive = false;
  this->httpConnType = http_http;
  this->websocketoutogingpingcount = 0;
  connection_count++;
  current_connection_count++;
}

/*******************************************************************************
Class: ~httpconnection
Description: Dstrcutor.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
nmbls::httpconnection::~httpconnection()
{
  std::cout << "httpconnection::~httpconnection" << std::endl;
  current_connection_count--;
}

/*******************************************************************************
Class: httpconnection::create
Description: Create a new object - used by asio.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
nmbls::httpconnection::pointer nmbls::httpconnection::create(boost::asio::io_service& io_service)
{
  return pointer(new httpconnection(io_service));
}

/*******************************************************************************
Class: httpconnection::socket
Description: Returns the underlying socket.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
boost::asio::ip::tcp::socket& nmbls::httpconnection::socket()
{
  return socket_;
}


/*******************************************************************************
Class: httpconnection::start
Description: Called by asio - to start a connection..
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::start()
{
  socket_.async_read_some( boost::asio::buffer( this->readbuffer.data(), MAXPAYLOADSIZE-1 ),
    boost::bind(&httpconnection::handleread, shared_from_this(),
    boost::asio::placeholders::error,
    boost::asio::placeholders::bytes_transferred));
}

/*******************************************************************************
Function: httpconnection::handleread
Description: Listen for new connections and spawn new object to handle.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::handleread(const boost::system::error_code& error, std::size_t bytes_transferred)
{
  if ( !error )
  {
    std::string Close;
    httpdoc ourDoc;
    // First test is, are we a web socket if so becomes very simple,
    // let's do this first so we become very efficient for our
    // websockets as this is the future of our presence server
    if ( http_websocket == this->httpConnType )
    {
      this->handlewspacket();
      goto readsomemore;
    }

    /* ensure our string is NULL terminated b4 we start using null term functions */
    this->readbuffer[ bytes_transferred ] = 0;
    ourDoc.parse( this->readbuffer.data() );

    Close = ourDoc.getheader( "connection" );
    std::transform( Close.begin(),Close.end(),Close.begin(),tolower );

    if ( Close == "keep-alive" )
    {
      this->KeepAlive = true;
    }
    else
    {
      this->KeepAlive = false;
    }

    if ( http_websocket != this->httpConnType && true == this->handlewsupgrade(ourDoc) )
    {
      std::cout << "Successfully handled upgrade." << std::endl;
      goto readsomemore;
    }
    this->handlerequest( ourDoc );

    readsomemore:
    // Boost asio seems to need this to keep the connection open. Regardless
    // of whether we are waiting for more data or not.
    socket_.async_read_some(boost::asio::buffer(this->readbuffer, this->readbuffer.size()-1),
        boost::bind(&httpconnection::handleread, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
  }
}

/*******************************************************************************
Class: httpconnection::handlerequest
Description: Handle the request we have received.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::handlerequest( httpdoc &in )
{
  /* then we move onto checking our command - we take the command from the filename we are given */
  std::string uri = in.geturi();
  /* This is where we hook into our handlers */

  nmbls::nmblshandler *handler = nmbls::gethandler( uri );
  std::cout << "handlerequest: " << uri << std::endl;

  if ( NULL == handler )
  {
    this->postnotfound();
    return;
  }

  try
  {
    httpdoc out;
    out.addheader( "content-type", "text/html; charset=utf-8" );

    handler->onhttp( in, out );
    this->postdocument( out.generatehttpdoc() );
  }
  catch( nmbls::httpexception e )
  {
    httpdoc out;
    this->postdocument( out.generatehttpdoc( e.code, e.message ) );
  }
  catch( ... )
  {
    httpdoc out;
    this->postdocument( out.generatehttpdoc( "500", "Unhandled error" ) );
  }

}

/*******************************************************************************
Class: httpconnection::handlewsupgrade
Description: Upgrade the connection to websocket.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
bool nmbls::httpconnection::handlewsupgrade(httpdoc &ourDoc)
{
  // RFC 6455
  // The next section is taken from 4.2.1 and includes the basic headers we must grab
  // to start the handshake
  std::string upgade = ourDoc.getheader("upgrade");
  std::transform(upgade.begin(),upgade.end(),upgade.begin(),tolower);
  if ( std::string::npos == upgade.find("websocket") )
  {
    return false;
  }
  std::cout << "Found 'Upgrade: websocket'" << std::endl;

  httpdoc ourDocument;
  // TODO - this is actually questionable - although needed but could use better control.
  ourDocument.addheader("Access-Control-Allow-Origin", "*");

  std::string connection = ourDoc.getheader("connection");
  std::transform(connection.begin(),connection.end(),connection.begin(),tolower);
  if ( std::string::npos == connection.find("upgrade") )
  {
    return false;
  }
  std::cout << "Found 'Connection: Upgrade'" << std::endl;

  // base 64 encoded which when decoded is 16 bytes long
  std::string sec_websocket_key = ourDoc.getheader("sec-websocket-key");
  std::string sec_websocket_key_dec = base64_decode(sec_websocket_key);
  if ( sec_websocket_key_dec.length() != 16 )
  {
    std::cout << "Uh oh, sec_websocket_key_dec is only: " << sec_websocket_key_dec.length() << std::endl;
    std::cout << sec_websocket_key << std::endl;
    return false;
  }
  std::cout << "We have a Sec-WebSocket-Key: " << sec_websocket_key << std::endl;

  // must be 13
  std::string sec_websocket_version = ourDoc.getheader("sec-websocket-version");
  if ( std::string::npos == sec_websocket_version.find("13"))
  {
    return false;
  }
  std::cout << "Sec-WebSocket-Version is the correct version (13)" << std::endl;

  // Now send the opening handshake in response
  ourDocument.addheader("Upgrade","websocket");
  ourDocument.addheader("Connection","Upgrade");

  std::string encoded_sec = sec_websocket_key + std::string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  boost::uuids::detail::sha1 s;
  s.process_bytes(encoded_sec.c_str(), encoded_sec.size());
  unsigned int digest[5];
  s.get_digest(digest);
  unsigned char hash[20];
  for(int i = 0; i < 5; ++i)
  {
    const unsigned char* tmp = reinterpret_cast<unsigned char*>(digest);
    hash[i*4] = tmp[i*4+3];
    hash[i*4+1] = tmp[i*4+2];
    hash[i*4+2] = tmp[i*4+1];
    hash[i*4+3] = tmp[i*4];
  }
  std::string sec_websocket_accept = base64_encode(hash, 20);

  // At this point, we may not auth the session
  // but we must deliver our responce back via web sockets...
  ourDocument.addheader("Sec-WebSocket-Accept",sec_websocket_accept);

  this->postdocument( ourDocument.generatehttpdoc( "101", "Switching protocols" ) );
  this->httpConnType = http_websocket;
  this->KeepAlive = true;
  /* At this point we are now considered open and can start sending and receiving
   websocket frames */

  return true;
}

/*******************************************************************************
Function: handlewspacket
Description: Handler for a websocket packet.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::handlewspacket( void )
{
  websocketframe ourReceivedFrame;
  ourReceivedFrame.parseframe(this->readbuffer);

  if ( ourReceivedFrame.isping() )
  {
    this->postpong();
  }
  else if ( ourReceivedFrame.ispong() )
  {
    if ( 0 != this->websocketoutogingpingcount )
    {
      // we should always get in here
      this->websocketoutogingpingcount--;
    }
  }
  else
  {
    // TODO - find our handler and pass it up the tree.
  }
}

/*******************************************************************************
Function: postpong
Description: Post a web socket pong
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postpong(void)
{
  charvector tosend = websocketframe::generatepongframe();

  websocketqueue.push_back( tosend );
  if ( 1 == this->websocketqueue.size() )
  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->websocketqueue[ 0 ] ),
        boost::bind( &httpconnection::handlewswrite, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
  }
}

/*******************************************************************************
Function: postping
Description: Posts a websocket ping packet.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postping(void)
{
  std::cout << "Sending ping packet" << std::endl;

  this->websocketoutogingpingcount++;

  if ( 4 < this->websocketoutogingpingcount )
  {
    this->socket_.close();
    return;
  }

  charvector tosend = websocketframe::generatepingframe();

  this->websocketqueue.push_back( tosend );
  if ( 1 == this->websocketqueue.size() )
  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->websocketqueue[ 0 ] ),
      boost::bind( &httpconnection::handlewswrite, shared_from_this(),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred ) );
  }
}

/*******************************************************************************
Class: httpconnection::postwstextframe
Description: Post websocket text frame. TODO - check it is a websocket!
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postwstextframe(std::string txt)
{
  charvector docbod = websocketframe::generatetextframe(txt);

  websocketqueue.push_back( docbod );
  if ( 1 == this->websocketqueue.size() )

  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->websocketqueue[ 0 ] ),
        boost::bind( &httpconnection::handlewswrite, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
  }
}

/*******************************************************************************
Class: httpconnection::handlewrite
Description: Called after a write has finished - http mode (not websocket).
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::handlewrite(const boost::system::error_code& error, std::size_t bytes_transferred)
{
  if ( error )
  {
    std::cerr << "Problem writing to socket" << boost::system::system_error(error).what() << std::endl;;
    return;
  }

  this->webqueue.pop_front();
  if ( this->webqueue.size() > 0 )
  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->webqueue[ 0 ] ),
        boost::bind( &httpconnection::handlewrite, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
  }

  if ( true != this->KeepAlive )
  {
    this->socket_.close();
  }
}

/*******************************************************************************
Class: httpconnection::handlewswrite
Description: Called after a write has finished - http mode (not websocket).
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::handlewswrite(const boost::system::error_code& error, std::size_t bytes_transferred)
{
  if ( error )
  {
    std::cerr << "Problem writing to socket" << boost::system::system_error(error).what() << std::endl;;
    return;
  }

  this->websocketqueue.pop_front();
  if ( this->websocketqueue.size() > 0 )
  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->websocketqueue[ 0 ] ),
        boost::bind( &httpconnection::handlewswrite, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred ) );
  }

  if ( true != this->KeepAlive )
  {
    this->socket_.close();
  }
}

/*******************************************************************************
Class: httpconnection::postdocument
Description: Listen for new connections and spawn new object to handle.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postdocument( std::string fullhttpresponce )
{
  this->webqueue.push_back( fullhttpresponce );
  std::cout << "Sending: " << fullhttpresponce << std::endl;
  if ( 1 == this->webqueue.size() )
  {
    boost::asio::async_write( socket_, boost::asio::buffer( this->webqueue[ 0 ] ),
      boost::bind( &httpconnection::handlewrite, shared_from_this(),
      boost::asio::placeholders::error,
      boost::asio::placeholders::bytes_transferred ) );
  }
}

/*******************************************************************************
Class: httpconnection::postunauthorized
Description: Post 401.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postunauthorized(std::string message, std::string realm)
{
  httpdoc ourDoc;
  ourDoc.addheader("WWW-Authenticate", realm);
  this->postdocument( ourDoc.generatehttpdoc( "401", message ) );
}

/*******************************************************************************
Class: nmbls
Description: Listen for new connections and spawn new object to handle.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::httpconnection::postnotfound(std::string message)
{
  httpdoc ourDocument;

  ourDocument.addheader("Content-Type","text/html; charset=utf-8");
  ourDocument.setbody(message);

  this->postdocument( ourDocument.generatehttpdoc( "404" ) );
}

/*******************************************************************************
Class: nmbls
Description: Listen for new connections and spawn new object to handle.
Date: 15.11.2017
Author: Nick Knight
*******************************************************************************/
std::string nmbls::httpconnection::CalcSha1(std::string input)
{
  std::stringstream output;
  output << std::hex << std::setfill('0');

  boost::uuids::detail::sha1 s;

  s.process_bytes(input.c_str(), input.size());
  unsigned int digest[5];
  s.get_digest(digest);
  for(int i = 0; i < 5; ++i)
  {
    output << std::setw(sizeof(unsigned int)*2)  << digest[i];
  }

  return output.str();
}
