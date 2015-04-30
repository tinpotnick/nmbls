/*
 * http_connection.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: Nick Knight
 *
 * See LICENCE for more details.
 */

#include <iostream>
#include <boost/uuid/sha1.hpp>

#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include <boost/thread/thread.hpp>

#include <boost/filesystem.hpp>

#include "http_connection.h"
#include "utils.h"
#include "base64.h"

#include "mem_measure.h"

#include "weblibs.h"
#include "nmbls.h"

#include <boost/unordered_set.hpp>

extern libraries ourloadedlibs;
extern bool is_running_as_debug;
extern boost::thread::id mainthreadid;
extern const std::string serverroot;

int num_connections = 0;

typedef boost::unordered_set<HttpConnection *> connection_tracker;
connection_tracker live_connections;

// TODO
// This probably needs to be made faster, can use hash map of some form...
ValuePair mimetypes;

void loadMimeTypes(void)
{
	std::string filename;
	filename = serverroot + boost::filesystem::path::preferred_separator + "mimetypes";


	// Thank you Apache
	// http://svn.apache.org/viewvc/httpd/httpd/branches/2.2.x/docs/conf/mime.types?revision=1576707&view=co

	std::ifstream infile;
	if ( boost::filesystem::is_regular_file(filename) )
	{
		infile.open(filename, std::ifstream::binary);

		std::string line;
		while (std::getline(infile, line))
		{
			if ( '#' == line[0] )
			{
				continue;
			}
			std::istringstream buffer(line);
			std::vector<std::string> parts{std::istream_iterator<std::string>(buffer),
                                 std::istream_iterator<std::string>()};

			auto it = parts.begin();
			std::string mimetype = *it;
			it++;
			for ( ; it != parts.end(); it++)
			{
				mimetypes[*it] = mimetype;
			}
		}
	}
}

///////////////////////////////////////////////////////////////
//
// Function HttpConnection
//
// C'Tor
//
///////////////////////////////////////////////////////////////
HttpConnection::HttpConnection(boost::asio::io_service& io_service)
								: socket_(io_service),
								timer_(io_service),
								io_service_(io_service)
{
	this->httpConnType = http_http;
	this->outstanding_ping_count = 0;

	num_connections++;

	live_connections.insert(this);
}


///////////////////////////////////////////////////////////////
//
// Function ~HttpConnection
//
// D-stor
//
///////////////////////////////////////////////////////////////
HttpConnection::~HttpConnection()
{
	std::cout << "~HttpConnection" << std::endl;

	libraries::iterator it;
	for ( it = ourloadedlibs.begin(); it != ourloadedlibs.end(); it++ )
	{
		it->second->DestroyConnection(this);
	}

	auto live_it = live_connections.find(this);
	if ( live_it != live_connections.end() )
	{
		live_connections.erase(live_it);
	}
}


///////////////////////////////////////////////////////////////
//
// Function create
//
// required by asio
//
///////////////////////////////////////////////////////////////
HttpConnection::pointer HttpConnection::create(boost::asio::io_service& io_service)
{
	return pointer(new HttpConnection(io_service));
}


///////////////////////////////////////////////////////////////
//
// Function socket
//
// returns our socket object
//
///////////////////////////////////////////////////////////////
boost::asio::ip::tcp::socket& HttpConnection::socket()
{
	return socket_;
}



///////////////////////////////////////////////////////////////
//
// Function start
//
// The very first thing called when a connection is made.
// as it is a HTTP server this simply sets a read operation up.
//
///////////////////////////////////////////////////////////////
void HttpConnection::start(void)
{
	socket_.async_read_some(boost::asio::buffer(buffer_, buffer_.size()-1),
		boost::bind(&HttpConnection::HandleRead, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

///////////////////////////////////////////////////////////////
//
// Function HandleWrite
//
// Called after an asyncronous write.
//
///////////////////////////////////////////////////////////////
void HttpConnection::HandleWrite(const boost::system::error_code& error, std::size_t bytes_transferred)
{

	if ( error )
	{
		std::cerr << "Problem writing to socket" << boost::system::system_error(error).what() << std::endl;;
		return;
	}

	this->tempLocalbuffer.pop_front();
	if ( this->tempLocalbuffer.size() > 0 )
	{
		boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
				boost::bind(&HttpConnection::HandleWrite, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
	}
}

static std::string genHttpDoc(std::string body)
{
	std::stringstream str;

	str << "Content-length: " << body.size() << "\r\n";
	str << "Connection: Keep-Alive\r\n\r\n";

	str << body;

	return str.str();
}
///////////////////////////////////////////////////////////////
//
// Function HandleRead
//
// Our main function. Every time a request is made on our HTTP
// server this call is made. The first thing we do is parse
// the GET string passed to us (we don't support POST yet).
// Contains a series of if statements to match against the
// filename requested, i.e. if http://server/Authenticate?...
// is called then look out for else if ( Filename == "Authenticate" )
//
///////////////////////////////////////////////////////////////
void HttpConnection::HandleRead(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if ( bytes_transferred > MAXPAYLOADSIZE )
	{
		std::cout << "Received data too large - ignoring" << std::endl;
		return;
	}

	if (!error)
	{
		// First test is, are we a web socket if so becomes very simple,
		// let's do this first so we become very efficient for our
		// websockets as this is the future of our presence server
		if ( http_websocket == this->httpConnType )
		{
			this->HandleWebSocketPacket();
		}
		else
		{
			// Ensure NULL terminated
			buffer_[bytes_transferred] = 0;
			std::string doc(buffer_.c_array());

			ValuePair empty;
			this->HttpQueryValues = empty;
			this->HttpHeaders = empty;

			boost::posix_time::ptime reference;
			measureuSTime(reference, is_running_as_debug);

			this->ParseRequest(doc);
			measureuSTime(reference, is_running_as_debug, "Parse HTTP request");

			this->ParseHttpHeaders(doc);
			measureuSTime(reference, is_running_as_debug, "Parse HTTP headers");

			std::string host = this->HttpHeaders["host"];
			if ( "" == host )
			{
				this->PostHttpDocument("404", "Not found", genHttpDoc("Well formed HTTP requests supply a host header?"));
			}
			if ( false == this->isAGoodFilename())
			{
				this->PostHttpDocument("400", "Bad request", genHttpDoc(std::string("Some questionable filename ") + this->RequestFile));
			}
			else
			{
				if ( false == this->HandleWebSocketUpgrade() )
				{
					if (is_running_as_debug) std::cout << "Not a websocket request..." << std::endl;
					// Now we perform normal get (or in the future post) requests
					libraries::iterator it;
					bool foundhandler = false;

					measureuSTime(reference, is_running_as_debug, "Looking in mod");
					if ( false == this->sendFile())
					{
						for ( it = ourloadedlibs.begin(); it != ourloadedlibs.end(); it++ )
						{
							if ( true == it->second->WebRequest(this->method, this, this->RequestFile, this->HttpQueryValues, this->HttpHeaders,
			                                                                      boost::bind(&HttpConnection::PostHttpDocument, shared_from_this(), _1, _2, _3)))
							{
								measureuSTime(reference, is_running_as_debug, "Found in mod");
								foundhandler = true;
								break;
							}
						}

						if ( !foundhandler )
						{
							std::cout << "Not found, sending 404" << std::endl;
							this->PostHttpDocument("404", "Not found", genHttpDoc("You are looking in the wrong place (try upstairs under the bed)..."));
						}
					}
				}
			}
		}

		// keep the connection open....
		socket_.async_read_some(boost::asio::buffer(buffer_, buffer_.size()-1),
						boost::bind(&HttpConnection::HandleRead, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
}


bool HttpConnection::isAGoodFilename(void)
{
	if ( std::string::npos !=  this->RequestFile.find(".."))
	{
		return false;
	}

	for ( auto it = this->RequestFile.begin(); it != this->RequestFile.end(); it++)
	{
		char c = *it;
		if ( ( (c>='a'&& c<='z') || (c>='A' && c<='Z') || (c>='0'&& c<='9')))
		{
			continue;
		}

		switch(c)
		{
			case '\\':
			case '-':
			case '_':
			case '.':
			case '/':
				continue;

			default:
				return false;
		}
	}
	return true;
}

bool HttpConnection::sendFile(void)
{

	if ( 0 != this->tempLocalbuffer.size() )
	{
		std::cerr << "That shouldn't happen, a file requeted with stuff still waiting to go" << std::endl;
		return false;
	}

	if ( this->serveFile.is_open())
	{
		std::cerr << "File shouldn't be already open - busy" << std::endl;
		return false;
	}

	std::string filename;

	filename = serverroot + boost::filesystem::path::preferred_separator;

	// To get here we must have got the host header
	std::string host = this->HttpHeaders["host"];
	filename += host + boost::filesystem::path::preferred_separator;

	filename += std::string("files") + boost::filesystem::path::preferred_separator;
	std::string hostroot = filename;
	filename += this->RequestFile;

	std::cout << "Looking for file '" << filename << "'" << std::endl;

	if ( !boost::filesystem::is_regular_file(filename) )
	{
		filename += "index.html";
		std::cout << "Nope, trying " << filename << " instead" << std::endl;
	}

	if ( boost::filesystem::is_regular_file(filename) )
	{
		this->serveFile.open(filename, std::ifstream::binary);
		if ( this->serveFile.fail())
		{
			std::cerr << "Error opening " << filename << std::endl;
		}
		else
		{
			this->serveFile.seekg (0, this->serveFile.end);
		    this->remainingFileSize = this->serveFile.tellg();
			this->serveFile.seekg (0,this->serveFile.beg);

			std::stringstream headers;

			headers << "HTTP/1.1 200 OK\r\n";

			std::string ext = boost::filesystem::extension(this->RequestFile);
			if ( ext.size() > 1  )
			{
				ext = ext.substr(1);

				std::cout << "Ext is: " << ext << std::endl;
				std::string mimetype = mimetypes[ext];

				headers << "Content-type: " << mimetype << "\r\n";
			}
			headers << "Content-length: " << this->remainingFileSize << "\r\n";
			headers << "Connection: Keep-Alive\r\n\r\n";

			std::cout << headers.str();

			charvector outdoc;
			std::string tmpbuf = headers.str();
			outdoc.reserve(tmpbuf.size());
			for (auto it = tmpbuf.begin(); it != tmpbuf.end(); it ++)
			{
				outdoc.push_back(*it);
			}

			this->tempLocalbuffer.push_back(outdoc);

			boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
				boost::bind(&HttpConnection::ContinueFile, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));

			return true;
		}
	}

	return false;

}

void HttpConnection::ContinueFile(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	this->tempLocalbuffer.pop_front();

	if ( error )
	{
		std::cerr << "Problem writing file to socket" << boost::system::system_error(error).what() << std::endl;;
		return;
	}

	if ( this->remainingFileSize <= 0 )
	{
		this->remainingFileSize = 0;
		this->serveFile.close();
		return;
	}

	std::streamoff currentamount = 1024;
	if ( this->remainingFileSize < currentamount )
	{
		currentamount = this->remainingFileSize;
	}

	char buf[currentamount+1];
	this->serveFile.read(buf, currentamount);

	charvector fileBuf;
	fileBuf.reserve(currentamount);

	for ( int i =0 ; i < currentamount; i++ )
	{
		fileBuf.push_back(buf[i]);
	}

	this->remainingFileSize -= currentamount;
	this->tempLocalbuffer.push_back(fileBuf);

	boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
		boost::bind(&HttpConnection::ContinueFile, shared_from_this(),
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));

}

///////////////////////////////////////////////////////////////
//
// Function HandleWebSocketUpgrade
//
// If we are not already a websocket, it maybe requested that
// an upgrade occurs - so we check for it here.
//
///////////////////////////////////////////////////////////////
bool HttpConnection::HandleWebSocketUpgrade(void)
{
	// RFC 6455
	// The next section is taken from 4.2.1 and includes the basic headers we must grab
	// to start the handshake
	std::string upgrade = this->HttpHeaders["upgrade"];
	std::transform(upgrade.begin(),upgrade.end(),upgrade.begin(),tolower);
	if ( std::string::npos == upgrade.find("websocket") )
	{
		if (is_running_as_debug) std::cout << "No upgrade header." << std::endl;
		return false;
	}

	std::string connection = this->HttpHeaders["connection"];
	std::transform(connection.begin(),connection.end(),connection.begin(),tolower);
	if ( std::string::npos == connection.find("upgrade") )
	{
		if (is_running_as_debug) std::cout << "No connection header." << std::endl;
		return false;
	}

	// base 64 encoded which when decoded is 16 bytes long
	std::string sec_websocket_key = this->HttpHeaders["sec-websocket-key"];
	std::string sec_websocket_key_dec = base64_decode(sec_websocket_key);
	if ( sec_websocket_key_dec.length() != 16 )
	{
		if (is_running_as_debug) std::cout << "Uh oh, sec_websocket_key_dec is only: " << sec_websocket_key_dec.length() << std::endl;
		if (is_running_as_debug) std::cout << sec_websocket_key << std::endl;
		return false;
	}

	// must be 13
	std::string sec_websocket_version = this->HttpHeaders["sec-websocket-version"];
	if ( std::string::npos == sec_websocket_version.find("13"))
	{
		if (is_running_as_debug) std::cout << "sec-websocket-version not correct: " << sec_websocket_version << std::endl;
		return false;
	}

	// Now send the opening handshake in response
	std::stringstream ourDocument;
	ourDocument << "Access-Control-Allow-Origin: *\r\n";

	ourDocument << "Upgrade: websocket\r\n";
	ourDocument << "Connection: Upgrade\r\n";

	std::string encoded_sec = sec_websocket_key + std::string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	// Mmmmm, borrowed code, doesn't look very portable....
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
	ourDocument << "Sec-WebSocket-Accept: " << sec_websocket_accept << "\r\n\r\n";

	this->PostHttpDocument("101", "Switching protocols", ourDocument.str());
	this->httpConnType = http_websocket;
	// At this point we are now considered open and can start sending and receiving
	// websocket frames

	if ( "" == this->RequestFile )
	{
		//if (is_running_as_debug) std::cout << "No request file..." << std::endl;
		//this->socket_.close();
		//return false;
	}

	socket_.async_read_some(boost::asio::buffer(buffer_, buffer_.size()-1),
								boost::bind(&HttpConnection::HandleRead, shared_from_this(),
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred));

	if (is_running_as_debug) std::cout << "Handled upgrade to websocket, mem used: " << toKB(getCurrentRSS()) << "KB" << std::endl;

	libraries::iterator it;
	for ( it = ourloadedlibs.begin(); it != ourloadedlibs.end(); it++ )
	{
		if ( true == it->second->NewConnection(this, this->RequestFile, this->HttpHeaders, this->HttpQueryValues,
                                                 boost::bind(&HttpConnection::PostWebsocketTextFrame, shared_from_this(), _1)))
		{
			break;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////
//
// Function ParseWebRequest
//
// Parse HTTP GET request including query string
//
///////////////////////////////////////////////////////////////
void HttpConnection::ParseRequest(std::string &getstr)
{
	std::size_t firstwhitespace = getstr.find(" ");

	if ( firstwhitespace == std::string::npos || firstwhitespace > 10 )
	{
		return;
	}

	this->method = getstr.substr(0, firstwhitespace);

	std::size_t endpos = getstr.find("HTTP", firstwhitespace);

	if ( std::string::npos == endpos )
	{
		return;
	}

	std::size_t paramspos = getstr.find('?', firstwhitespace);
	if ( std::string::npos != paramspos )
	{
		this->GetRequest = getstr.substr(paramspos,endpos - paramspos-1);
	}

	if ( std::string::npos == paramspos )
	{
		this->RequestFile = getstr.substr(firstwhitespace+2, endpos-firstwhitespace-3);
	}
	else
	{
		this->RequestFile = getstr.substr(firstwhitespace+2, paramspos - (firstwhitespace+2));
	}

	if ( '/' == this->RequestFile[0] )
	{
		this->RequestFile.erase(0,1);
	}

	if ( std::string::npos != paramspos )
	{
		std::string params = getstr.substr(paramspos+1, endpos-paramspos-2);

		while ( params.size() != 0 )
		{
			std::string paramname;
			std::string paramvalue;

			std::size_t paramendpos = params.find('&', 0);
			std::string thisparam;

			if ( std::string::npos == paramendpos )
			{
				thisparam = params;
				params = "";
			}
			else
			{
				thisparam = params.substr(0, paramendpos);
				params.erase(0, paramendpos+1);
			}

			std::size_t paramsplit = thisparam.find('=', 0);

			if ( std::string::npos == paramsplit )
			{
				paramname = thisparam;
				paramvalue = "";
			}
			else
			{
				paramname = thisparam.substr(0,paramsplit);
				thisparam.erase(0,paramsplit+1);
				paramvalue = thisparam;
			}

			std::cout << "Adding query param: " << paramname << ":" << paramvalue << std::endl;
			this->HttpQueryValues[paramname] =  paramvalue;
		}
	}
}


///////////////////////////////////////////////////////////////
//
// Function ParseHttpHeaders
//
// Parse HTTP headers we receive in the request
//
///////////////////////////////////////////////////////////////
void HttpConnection::ParseHttpHeaders(std::string &getstr)
{
	std::size_t endofheaders = getstr.find("\r\n\r\n");

	if ( std::string::npos == endofheaders)
	{
		return;
	}

	std::string headers = getstr.substr(0, endofheaders+2);
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

		TrimRight(value, " \r\n\t");
		TrimLeft(value, " \r\n\t");

		TrimRight(param, " \r\n\t");
		TrimLeft(param, " \r\n\t");

		std::transform(param.begin(),param.end(),param.begin(),tolower);

		this->HttpHeaders[param] = value;
		//if (is_running_as_debug) std::cout << "this->HttpHeaders[" << param << "] = " << value << std::endl;

		headers = headers.substr(endofline + 2 , headers.size());
	}
}

///////////////////////////////////////////////////////////////
//
// Function HandleWebSocketPacket
//
// When we upgrade to a websocket we will receive our packets
// here. We also handle pings and pongs here.
//
///////////////////////////////////////////////////////////////
void HttpConnection::HandleWebSocketPacket(void)
{
#if 0
	bool fin = false;
	if ( (buffer_[0] & 0x80) == 0x80 )
	{
		fin = true;
	}
#endif

	/*
	 * op code:
	 * 0x0: continuation frame
	 * 0x1: text frame
	 * 0x2: binary frame
	 * 0x3-7: reserved for further control frames
	 * 0x8: connection close
	 * 0x9: ping
	 * 0xa: pong
	 * 0xb-f: reserved for further control frames
	 */
	op_codes op_code = op_code_reserved;
	switch (buffer_[0] & 0x0f)
	{
	case 0x0:
		op_code = op_code_continuation;
		break;
	case 0x1:
		op_code = op_code_text;
		break;
	case 0x2:
		op_code = op_code_binary;
		break;
	case 0x8:
		op_code = op_code_close;
		break;
	case 0x9:
		op_code = op_code_ping;
		break;
	case 0xa:
		op_code = op_code_pong;
		break;
	default:
		op_code = op_code_reserved;
		break;
	}

	boost::uint64_t payload_len = (buffer_[1] & 0x7f);
	bool mask = (buffer_[1] & 0x80) == 0x80;

	int data_ptr = 2;

	switch ( payload_len )
	{
	case 126:
		/* the next 16 bit unsigned is payload length */

		payload_len = (boost::uint64_t(buffer_[2]) & 0xff) << 8 |
							(boost::uint64_t(buffer_[3]) & 0xff);
		data_ptr = 4;
		break;
	case 127:
		/* the next 8 bytes - 64 bit unsigned payload length */
		payload_len = (boost::uint64_t(buffer_[2]) & 0xff) << 56 |
							(boost::uint64_t(buffer_[3]) & 0xff) << 48 |
							(boost::uint64_t(buffer_[4]) & 0xff) << 40 |
							(boost::uint64_t(buffer_[5]) & 0xff) << 32 |
							(boost::uint64_t(buffer_[6]) & 0xff) << 24 |
							(boost::uint64_t(buffer_[7]) & 0xff) << 16 |
							(boost::uint64_t(buffer_[8]) & 0xff) << 8 |
							(boost::uint64_t(buffer_[9]) & 0xff);
		data_ptr = 10;
		break;
	default:
		/* leave it as it is */
		break;
	}

	char masking_key[4];


	if ( true == mask )
	{
		masking_key[0] = buffer_[data_ptr];
		masking_key[1] = buffer_[data_ptr+1];
		masking_key[2] = buffer_[data_ptr+2];
		masking_key[3] = buffer_[data_ptr+3];

		data_ptr += 4;
	}

	switch(op_code)
	{
	case op_code_ping:
		this->HandlePing();
		break;
	case op_code_pong:
		this->HandlePong();
		break;


	/* our implementation will not require very large packets
	 * so we can assume we will not have any continuation frames
	 */
	case op_code_text:
	{
		std::string bodytext;
		char * ptr = buffer_.c_array() + data_ptr;

		if (true == mask)
		{
			for (uint64_t i = 0; i < payload_len; i++)
			{
				bodytext += (*ptr) xor masking_key[i % 4];
				ptr++;
			}
		}
		else
		{
			for (uint64_t i = 0; i < payload_len; i++)
			{
				bodytext += *ptr;
				ptr++;
			}
		}

		libraries::iterator it;
		for ( it = ourloadedlibs.begin(); it != ourloadedlibs.end(); it++ )
		{
			if ( true == it->second->TextPacket(this, bodytext))
			{
				break;
			}
		}


		break;
	}

        case op_code_close:
            this->socket_.close();
            return;
            break;
	default:
		// Uh oh...
		std::cerr << "Handling unexpected type of websocket packet" << std::endl;
		break;

	}
}

///////////////////////////////////////////////////////////////
//
// Function HandlePing
//
// As it says....
//
///////////////////////////////////////////////////////////////
void HttpConnection::HandlePing(void)
{
	charvector pongpacket;

	pongpacket[0] = 0x8a;
	pongpacket[1] = 0x00;

	// Now do something with the packet.
	this->tempLocalbuffer.push_back(pongpacket);

	if ( 1 == this->tempLocalbuffer.size() )
	{
		boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
			boost::bind(&HttpConnection::HandleWrite, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
}


///////////////////////////////////////////////////////////////
//
// Function PostPing
//
// As it says....
//
///////////////////////////////////////////////////////////////
void HttpConnection::PostPing(void)
{
	if ( this->outstanding_ping_count > 3 )
	{
		this->socket_.close();
		return;
	}

	charvector pingpacket;

	pingpacket[0] = 0x9;
	pingpacket[1] = 0x00;

	this->outstanding_ping_count++;

	// Now do something with the packet.
	this->tempLocalbuffer.push_back(pingpacket);

	if ( 1 == this->tempLocalbuffer.size() )
	{
		boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
			boost::bind(&HttpConnection::HandleWrite, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
}
///////////////////////////////////////////////////////////////
//
// Function HandlePong
//
// As it says
//
///////////////////////////////////////////////////////////////
void HttpConnection::HandlePong(void)
{
	this->outstanding_ping_count = 0;
}

///////////////////////////////////////////////////////////////
//
// Function PostWebsocketTextFrame
//
// Sends text packet to client
//
///////////////////////////////////////////////////////////////
void HttpConnection::PostWebsocketTextFrame(std::string text)
{
	charvector frame;
	size_t sz = text.length();

	// Quick check, payload plus some space for headers
	if ( (MAXPAYLOADSIZE - 10 ) < sz )
	{
		return;
	}

	if ( http_websocket != this->httpConnType )
	{
		return;
	}

	// FIN, RSV, opcode
	frame.push_back(0x81);

	if ( sz < 126 ) // 125 > sz )
	{
		frame.push_back(sz & 0x7f);
	}
	else if ( 0xffff >= sz )
	{
		frame.push_back(126);

		frame.push_back(sz >> 8);
		frame.push_back(sz & 0xff);
	}
	else
	{
		frame.push_back(127);

		frame.push_back(sz >> 56);
		frame.push_back(sz >> 48 & 0xff);
		frame.push_back(sz >> 40 & 0xff);
		frame.push_back(sz >> 32 & 0xff);
		frame.push_back(sz >> 24 & 0xff);
		frame.push_back(sz >> 16 & 0xff);
		frame.push_back(sz >> 8 & 0xff);
		frame.push_back(sz & 0xff);
	}

	// Currently not supported is the mask, what do we need it for anyway - it is
	// hardly encryption...
	std::string::iterator it;

	for ( it = text.begin(); it != text.end(); it++)
	{
		frame.push_back(*it);
	}

	// Now do something with the frame.
	this->tempLocalbuffer.push_back(frame);

	if ( 1 == this->tempLocalbuffer.size() )
	{
		boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
			boost::bind(&HttpConnection::HandleWrite, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}

}

///////////////////////////////////////////////////////////////
//
// Function PostWebsocketTextFrame
//
// Sends text packet to client
//
///////////////////////////////////////////////////////////////
void HttpConnection::PostHttpDocument(std::string statuscode, std::string reasonphrase, std::string httpdoc)
{
	if ( mainthreadid != boost::this_thread::get_id() )
	{
		std::cout << "Request came from a different thread, posting to current thread" << std::endl;
		// Post it back to the io service so it can be handled by the main thread.
		this->io_service_.post( boost::bind( &HttpConnection::PostHttpDocument, shared_from_this(),
																			statuscode,
																			reasonphrase,
																			httpdoc
																		) );
		return;
	}

	// quick check, becuase this can be called from a different thread, this
	// thread may have closed the conn
	auto live_it = live_connections.find(this);
	if ( live_it == live_connections.end() )
	{
		// This object has been destroyed?
		return;
	}

	if ( !this->socket_.is_open() )
	{
		return;
	}

	std::string statusline = "HTTP/1.1 " + statuscode + " " + reasonphrase + "\r\n";
	charvector fulloutdoc;

	// Copy to the output buffer
	const char * srcptr = statusline.c_str();
	unsigned int len = statusline.length();
	for (unsigned int i = 0; i != len; i++)
	{
		fulloutdoc.push_back(*srcptr);
		srcptr++;
	}

	srcptr = httpdoc.c_str();
	len = httpdoc.length();
	for (unsigned int i = 0; i != len; i++)
	{
		fulloutdoc.push_back(*srcptr);
		srcptr++;
	}

	this->tempLocalbuffer.push_back(fulloutdoc);
std::cout <<  this->tempLocalbuffer.size() << std::endl;
	if ( 1 == this->tempLocalbuffer.size() )
	{
		boost::asio::async_write(socket_,boost::asio::buffer(this->tempLocalbuffer[0]),
			boost::bind(&HttpConnection::HandleWrite, shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
}

std::string HttpConnection::GetRequestFilename(void)
{
	return this->RequestFile;
}

std::string HttpConnection::GetQueryString(void)
{
	return this->GetRequest;
}

std::string HttpConnection::GetVirtualHost(void)
{
        return this->HttpHeaders["host"];
}
