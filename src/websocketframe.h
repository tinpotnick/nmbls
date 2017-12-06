
#ifndef WEBSOCKETFRAME_H_
#define WEBSOCKETFRAME_H_

#include <vector>
#include <string>
#include <stdint.h>

#include <boost/cstdint.hpp>

#include "httpconnection.h"

#if defined(__linux__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#endif

/* Implements RFC 6455 */
namespace nmbls
{
  class websocketframe
  {
  private:

  	nmbls::charvector frameData;

  	/* flags and parts of frame */
  	bool fin;
  	/* RSV 1 2 3 not needed. */
  	bool mask;

  	/* we will parse extended length into here if required */
  	boost::uint64_t payload_len;
  	char masking_key[4];

  	enum op_codes { op_code_continuation, op_code_text, op_code_binary, op_code_close, op_code_ping, op_code_pong, op_code_reserved };
  	op_codes op_code;

  	std::string bodytext;

  public:
  	websocketframe();
  	void parseframe( nmbls::charvector data );
  	std::string gettext(void);

  	static nmbls::charvector generatetextframe( std::string txt );
  	static nmbls::charvector generatepongframe();
  	static nmbls::charvector generatepingframe();

  	op_codes getopcode(void);
  	inline bool iscontinuation() { return op_code_continuation == this->op_code;}
  	inline bool istext() { return op_code_text == this->op_code;}
  	inline bool isbinary() { return op_code_binary == this->op_code;}
  	inline bool isclose() { return op_code_close == this->op_code;}
  	inline bool isping() { return op_code_ping == this->op_code;}
  	inline bool ispong() { return op_code_pong == this->op_code;}

  };
}

#endif
