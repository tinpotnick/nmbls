# NMBLS

## A Nimble Web Server.

A very fast, compact, efficient web server written in C++. Can serve static files as well data. The design pushes more functionality to HTML5 whilst providing back end functions like files, data, security and communications (websockets). Designed for CRUD applications as well as frameworks like angular.js.

The source in git at the moment is a working version, but very early with no documentation. It can handle GET requests and server files and has one module written to serve data from an SQLite database. Conversion to JSON happens in the server so is not passed off to any script to slow down performance.

Our design philosophy

* Designed around HTML5 in the client, making more use of the client including real time communications via websockets
* IO completion ports (see C10K below)
* Push the hardwork to the client
* Separate data and views
* Wherever possible, the web server should not handle state information (websockets are different and I will add documentation regarding that when this work is done)
* Add a layer for security (the security has to be worked on yet)

### Why push out the work to the client

Frameworks, like angular.js, make it easy to build good Model View Controller applications.

The capability both in terms of performance and functionality of a modern browser compared to that of one 10 years ago is amazing. If we have 1000 clients all requesting pages from our web server. It is much less work for the server to serve static files (HTML and JS etc) and data (JSON) than expecting the server to format and build the view and maintain state and so on. So if we have 1000 clients and 1 server, we push out more work we have 1001 computers at work instead of 1 with very dumb views.

## Timing nmbls on a micro EC2 instance:

* One table in a SQLite DB with contact information (first name, last name and phone number)
* Compare nmbls with nginx connecting to PHP-FPM (FastCGI Process Manager)
* test.php was written in around 10 lines of code to connect to SQLite DB and then format into a JSON format for client
* nmbls has all the logic in C++ to connect to DB and output data in JSON

NMBLS served 3 rows of data in ~260uS compared to ~500uS with nginx/PHP-FPM. nmbls was considerably simpler to setup. Calling session_start() in php increased this to ~650uS so the processing time increases quickly depending on what appears in the script.

BTW, it would be good to get the ~250uS even fast - which I think is doable. If there are any performance experts out there with any pointers...

Whilst nginx is build for the C10K problem, PHP is not. PHP is great for rapid development, solving most problems on most sites - but will never scale that well. nmbls will not suite all sites, but the sites it suites it should blow PHP out of the water.

nmbls hopes to solve the C10K problem, but also introduces better design into web sites so that they may scale even better.

* Separate views (html) with data

## C10K

nmbls was designed to scale well. As well as enforcing the design pattern onto the web developer to improve scalability, nmbls is written using IO completion ports. We never create 1 thread per connection, handling requests as they come in without blocking. When we need to handle data between threads we ensure we use  techniques to reduce the number of semaphores required in passing data between threads.

Examples, to come soon, watch this space web site to come soon.

Thank you for reading

Nick Knight
