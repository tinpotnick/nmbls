



#ifndef HTTPS_H_
#define HTTPS_H_

#include <boost/asio/ssl.hpp>

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

///////////////////////////////////////////////////////////////
//
// Class https_endpoint
//
// This is a simple class to listen out for new connections
// for our main server then creating new objects
// to handle the requests.
//
///////////////////////////////////////////////////////////////
class https_endpoint
{
public:
	https_endpoint(boost::asio::io_service& io_service, boost::asio::ssl::context& context);

	~https_endpoint();


        ssl_socket::lowest_layer_type& socket(void);
        void start(void);

	void handle_accept(HttpConnection::pointer new_connection,
		const boost::system::error_code& error);
        
        void handle_handshake(const boost::system::error_code& error);
        
        void handle_read(const boost::system::error_code& error,
                            size_t bytes_transferred);
        
        void handle_write(const boost::system::error_code& error);
        
private:
        ssl_socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
        

};


class https_server
{
public:
    https_server(boost::asio::io_service& io_service, unsigned short port);


    std::string get_password() const
    {
        return "";
    }

  void handle_accept(https_endpoint* new_session,
      const boost::system::error_code& error);

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ssl::context context_;
};


#endif /* HTTPS_H_ */



