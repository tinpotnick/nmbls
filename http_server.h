



#ifndef HTTP_H_
#define HTTP_H_



///////////////////////////////////////////////////////////////
//
// Class http_server
//
// This is a simple class to listen out for new connections
// for our main server then creating new objects
// to handle the requests.
//
///////////////////////////////////////////////////////////////
class http_server
{
public:
	http_server(boost::asio::io_service& io_service,
			boost::asio::ip::tcp::endpoint &our_endpoint);

	~http_server();

private:
	void start_accept(void);

	void handle_accept(HttpConnection::pointer new_connection,
		const boost::system::error_code& error);

	boost::asio::ip::tcp::acceptor acceptor_;
};



#endif /* HTTP_H_ */
