/*
 * https_server.cpp
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
#include "https_server.h"


https_endpoint::https_endpoint(boost::asio::io_service& io_service, boost::asio::ssl::context& context)
    : socket_(io_service, context)
{
}

https_endpoint::~https_endpoint()
{

}

ssl_socket::lowest_layer_type& https_endpoint::socket(void)
{
    return socket_.lowest_layer();
}


void https_endpoint::handle_handshake(const boost::system::error_code& error)
{
    if (!error)
    {
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                boost::bind(&https_endpoint::handle_read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        delete this;
    }
}

void https_endpoint::start(void)
{
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
            boost::bind(&https_endpoint::handle_handshake, this,
            boost::asio::placeholders::error));
}

void https_endpoint::handle_read(const boost::system::error_code& error,
                            size_t bytes_transferred)
{
    if (!error)
    {
        boost::asio::async_write(socket_,
                boost::asio::buffer(data_, bytes_transferred),
                boost::bind(&https_endpoint::handle_write, this,
                boost::asio::placeholders::error));
    }
    else
    {
        delete this;
    }
}

void https_endpoint::handle_write(const boost::system::error_code& error)
{
    if (!error)
    {
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
                boost::bind(&https_endpoint::handle_read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        delete this;
    }
}


https_server::https_server(boost::asio::io_service& io_service, unsigned short port)
    : io_service_(io_service),
        acceptor_(io_service,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
        context_(io_service, boost::asio::ssl::context::sslv23)
{
        context_.set_options(
                boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2
                | boost::asio::ssl::context::single_dh_use);

        context_.set_password_callback(boost::bind(&https_server::get_password, this));
        //context_.use_certificate_chain_file("server.pem");
        context_.use_private_key_file("server.pem", boost::asio::ssl::context::pem);
        //context_.use_tmp_dh_file("dh512.pem");

        https_endpoint* new_session = new https_endpoint(io_service_, context_);
        acceptor_.async_accept(new_session->socket(),
                boost::bind(&https_server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
}


void https_server::handle_accept(https_endpoint* new_session,
      const boost::system::error_code& error)
{
    if (!error)
    {
        new_session->start();
        new_session = new https_endpoint(io_service_, context_);
        acceptor_.async_accept(new_session->socket(),
                boost::bind(&https_server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
    }
    else
    {
        delete new_session;
    }
}
