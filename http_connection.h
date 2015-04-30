/*
 * http_connection.h
 *
 *  Created on: Nov 21, 2014
 *      Author: nick
 */

#ifndef HTTP_CONNECTION_H_
#define HTTP_CONNECTION_H_

#include <boost/enable_shared_from_this.hpp>
#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <boost/asio/deadline_timer.hpp>

#include <map>
#include <string>
#include <deque>
#include <vector>

#include <fstream>
#include <iostream>

#include "weblibs.h"

#define MAXPAYLOADSIZE 512
typedef boost::array<char, MAXPAYLOADSIZE> rawDataArray;
typedef std::vector<char> charvector;
typedef std::deque<charvector> queueCharVectors;


///////////////////////////////////////////////////////////////
//
// Class HttpConnection
//
// Represents a connection to a client using http. one is created
// for every connection made.
//
///////////////////////////////////////////////////////////////
class HttpConnection
	: public boost::enable_shared_from_this<HttpConnection>
{
public:
	typedef boost::shared_ptr<HttpConnection> pointer;

	static pointer create(boost::asio::io_service& io_service);
	boost::asio::ip::tcp::socket& socket();
	void start(void);

	HttpConnection(boost::asio::io_service& io_service);
	~HttpConnection();

	void PostWebsocketTextFrame(std::string text);
	void PostHttpDocument(std::string statuscode, std::string reasonphrase, std::string httpdoc);

	std::string GetRequestFilename(void);
	std::string GetQueryString(void);
    std::string GetVirtualHost(void);
	boost::asio::ip::tcp::socket socket_;
	boost::asio::deadline_timer timer_;
	boost::asio::io_service &io_service_;
	/// Buffer for incoming data.
	rawDataArray buffer_;


private:

	void HandleWrite(const boost::system::error_code& error, std::size_t bytes_transferred);
	void HandleRead(const boost::system::error_code& error, std::size_t bytes_transferred);
	void ParseRequest(std::string &getstr);
	void ParseHttpHeaders(std::string &getstr);
	void HandleWebSocketPacket(void);
	void HandlePing(void);
	void HandlePong(void);

	bool HandleWebSocketUpgrade(void);
	void PostPing(void);

	bool isAGoodFilename(void);
	bool sendFile(void);
	void ContinueFile(const boost::system::error_code& error, std::size_t bytes_transferred);

	enum httpconnectiontypes { http_http, http_websocket };
	enum op_codes { op_code_continuation, op_code_text, op_code_binary, op_code_close, op_code_ping, op_code_pong, op_code_reserved };

	httpconnectiontypes httpConnType;
	ValuePair HttpHeaders;
	ValuePair HttpQueryValues;
	std::string RequestFile;
	std::string GetRequest;
	std::string method;

	std::streamoff remainingFileSize;
	std::ifstream serveFile;

	int outstanding_ping_count;

	// Outqueue
	queueCharVectors tempLocalbuffer;

};


const char * _getRequestQueryString(websockcon conn);
const char * _getRequestFilename(websockcon conn);
void _postWebSocketText(websockcon conn, const char *str);
const char * _getVirtualHostName(websockcon conn);

void loadMimeTypes(void);


#endif /* HTTP_CONNECTION_H_ */
