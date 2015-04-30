/*
 * http_server.cpp
 *
 *  Created on: Nov 21, 2014
 *      Author: Nick Knight
 *
 * See LICENCE for more details.
 */

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/asio.hpp>

#include "http_connection.h"
#include "http_server.h"


http_server::http_server(boost::asio::io_service& io_service,
			boost::asio::ip::tcp::endpoint &our_endpoint)
    : acceptor_(io_service, our_endpoint)
{
    this->start_accept();
}

http_server::~http_server()
{

}

void http_server::start_accept(void)
{
        HttpConnection::pointer new_connection =
                HttpConnection::create(acceptor_.get_io_service());

        acceptor_.async_accept(new_connection->socket(),
                boost::bind(&http_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
}

void http_server::handle_accept(HttpConnection::pointer new_connection,
		const boost::system::error_code& error)
{
        if (!error)
        {
                new_connection->start();
        }
        else
        {
                std::cout << "Dang, shouldnt't have got here: " << error.message() << std::endl;
                std::cout << "ulimit -n 10000 - this can increase the number of connections required" << std::endl;
        }
		this->start_accept();

}
