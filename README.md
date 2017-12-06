
# Intro

The idea of nmbls is to make it easier to use C++ to build web applications (server side). Using a compiled language can be harder than using languages such as PHP or Python, but there are plenty of benefits such as a massive speed and performance improvement.

This framework currently (this is early development at the moment), supports GET/POST/DELETE requests and Websocket connections. My goal is to build a framework which is simple to use and provides enough tools to make building applications easier.

I have personally written a Websocket server based on nmbls which uses in memory data which can serve 10,000s of clients with updates as they happen.

# Speed Improvements

First, why do we need speed improvements? If the web server is running a little slow, then just bump up that instance size on AWS. Keep adding more instances until we can handle the load.

Sure, why not:

* It's only the bosses money - I don't care about that,
* Environment - pah,

Or, I could take on the challenge of having a web application which is as efficient as it can be, save a bit of the planet at the same time and to boot give my users the best possible experience.

There has been a change recently with web site generators growing in popularity. I am a big fan of them. If you have a blog, or documentation web site where the pages do not change - they should absolutely be generated only once. The speed of a page load of a pre generated HTML page is amazing over PHP.

But sitting alongside that you need something to serve dynamic content. Whether that is an API for your custom application or dynamic data on a web page then this is where nmbls comes in.

Compiled languages run at 10-20x faster than interpreted languages. The memory footprint is lower. We can also pay attention much more to the structure of how we serve content (take Nginx for example, there is a structural reason it can serve 20,000 clients).

What's more it is easier to deploy. With a PHP application, on your web server you need to install PHP, all the modules required for your application then all of the files in your applications - sometimes into the 100s/1000s.

# Why isn't C++ popular now in Web Servers?

They are for the big boys. But ultimately, when you are a web consultant, paid to put a web site together. Customers don't care about this stuff. They want their web site now.

PHP is popular because it is easy and it works (and it works well - I use it a lot). I have to make nmbls as easy to use as PHP. It has to be easy to write an application which will run faster without much thought as its PHP counterpart.

# Application Structure

Historically most web applications have a structure using the LAMP stack. HTML pages are built on the fly every time the page is requested, for example

```PHP
<!DOCTYPE html>
<html lang="en">
  <head>
    <!-- head stuff -->
  </head>
  <body>
    Welcome to our web site <?php echo $firstname; ?>.
  </body>
</html>
```

10,000 users request this file and your web server has to generate this 10,000 times. I mentioned above web site generators (such as Hugo) can generate web sites which get served by Nginx with ease. If the data and the page merge is separated so that the web server serves a HTML page, within the HTML page is JS to get the data. nmbls then serves the data. As it doesn't have to build the data into a HTML page this saves a lot of work on the server. So not only are we seeing the improvements of a compiled language we have structural improvements also.

If you have 10,000 clients, that is a potential 10,000 computers you can farm work off to. As much work should be passed off to the client as possible.

Note, there are situations where you do need to server content based on templates - which is how PHP is generally used. So nmbls will support template generation. At babblevoice I use this style to serve configuration data to VOIP phones.

# Getting started

When compiled, nmbls runs as a standalone web server. It uses boost::asio for efficient socket handling.

In the src directory - run make install which will build and install nmbls.

I designed nmbls to sit behind a proxy such as Nginx.

## Dependancies

We make use of boost lib.

Debian:
apt-get install libboost-all-dev

## Build

Go to the src directory and 'make install'. You can then build and use the test project.

## Hello World

This is first attempt at structure. I would like to get it even simpler. But this creates a web page which responds to the URI '/'.

```C++
#include <stdio.h>
#include "nmbls.h"

class roothandler : public nmbls::nmblshandler
{
public:
  roothandler() : nmbls::nmblshandler()
  {
    // Not needed but as an example.
    this->uri = "";
  }

  virtual void on( httpdoc &in, httpdoc &out )
  {
    out.body << "Hello World";
  }
};

int main( int argc, const char* argv[] )
{
  nmbls::addhandler( new roothandler() );
  nmbls::startserer( argc, argv );
  return 0;
}
```

This example can be found in the project sub directory. compile it using make. This will create the project executable. This can be run with ./project --debug if you want to see what is going on. If you run ./project it will daemonize the program.

# Timing examples

To come.

# TODO

I only have the basic stuff there yet. I want to add database and template support plus lots of other things. The example above uses a single thread as part of Asio (please read up on Asio and IO completion ports). Which makes it amazing speed wise but you cannot perform any calls which are blocking in this style. So I also need to add support of how this will work.

Timing reporting is also required.

So, some thinking required!
