

#include <iostream>
#include "websocketframe.h"

nmbls::websocketframe::websocketframe()
{
	this->mask = true;
	this->payload_len = 0;
	this->fin = false;
	this->masking_key[0] = 0;
	this->masking_key[1] = 0;
	this->masking_key[2] = 0;
	this->masking_key[3] = 0;

	this->op_code = op_code_reserved;
}

nmbls::charvector nmbls::websocketframe::generatetextframe(std::string txt)
{
	nmbls::charvector retval;

	// Quick check, payload plus some space for headers
	if ( (MAXPAYLOADSIZE - 10 ) < txt.size() )
	{
		return retval;
	}

	// FIN, RSV, opcode
	retval[0] = 0x81;

	boost::uint64_t sz = txt.size();
	std::cout << "Payload size is " << sz << std::endl;
	int ptr = 2;

	if ( sz < 126 ) // 125 > sz )
	{
		retval[1] = sz & 0x7f;
		std::cout << "Require just 7 bits for size info" << std::endl;
	}
	else if ( 0xffff >= sz )
	{
		retval[1] = 126;

		retval[2] = sz >> 8;
		retval[3] = sz & 0xff;
		ptr = 4;
		std::cout << "Require just 16 bits for size info" << std::endl;
	}
	else
	{
		retval[1] = 127;

		retval[2] = sz >> 56;
		retval[3] = sz >> 48 & 0xff;
		retval[4] = sz >> 40 & 0xff;
		retval[5] = sz >> 32 & 0xff;
		retval[6] = sz >> 24 & 0xff;
		retval[7] = sz >> 16 & 0xff;
		retval[8] = sz >> 8 & 0xff;
		retval[9] = sz & 0xff;
		ptr = 10;
		std::cout << "Require just 64 bits for size info" << std::endl;
	}

	// Currently not supported is the mask, what do we need it for anyway - it is
	// hardly encryption...
	std::string::iterator it;
	for ( it = txt.begin(); it != txt.end(); it++)
	{
		retval[ptr] = *it;
		ptr++;
	}

	return retval;
}

nmbls::charvector nmbls::websocketframe::generatepongframe()
{
	charvector retval;

	retval[0] = 0x8a;
	retval[1] = 0x00;

	return retval;
}

nmbls::charvector nmbls::websocketframe::generatepingframe()
{
	charvector retval;

	retval[0] = 0x89;
	retval[1] = 0x00;

	return retval;
}


std::string nmbls::websocketframe::gettext(void)
{
	if ( op_code_text == this->op_code )
	{
		return this->bodytext;
	}

	return "";
}

nmbls::websocketframe::op_codes nmbls::websocketframe::getopcode(void)
{
	return this->op_code;
}

void nmbls::websocketframe::parseframe(charvector data)
{
	/* rawDataArray is in unsigned 8 bit */
	std::cout << "Parsing websocket frame" << std::endl;

	this->frameData = data;

	this->fin = false;
	if ( (data[0] & 0x80) == 0x80 )
	{
		this->fin = true;
		std::cout << "fin=true" << std::endl;
	}

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

	switch (data[0] & 0x0f)
	{
	case 0x0:
		this->op_code = op_code_continuation;
		break;
	case 0x1:
		this->op_code = op_code_text;
		break;
	case 0x2:
		this->op_code = op_code_binary;
		break;
	case 0x8:
		this->op_code = op_code_close;
		break;
	case 0x9:
		this->op_code = op_code_ping;
		break;
	case 0xa:
		this->op_code = op_code_pong;
		break;
	default:
		this->op_code = op_code_reserved;
		break;
	}

	this->payload_len = (data[1] & 0x7f);
	this->mask = (data[1] & 0x80) == 0x80;

	int data_ptr = 2;

	switch ( this->payload_len )
	{
	case 126:
		/* the next 16 bit unsigned is payload length */

		this->payload_len = (boost::uint64_t(data[2]) & 0xff) << 8 |
							(boost::uint64_t(data[3]) & 0xff);
		data_ptr = 4;
		break;
	case 127:
		/* the next 8 bytes - 64 bit unsigned payload length */
		this->payload_len = (boost::uint64_t(data[2]) & 0xff) << 56 |
							(boost::uint64_t(data[3]) & 0xff) << 48 |
							(boost::uint64_t(data[4]) & 0xff) << 40 |
							(boost::uint64_t(data[5]) & 0xff) << 32 |
							(boost::uint64_t(data[6]) & 0xff) << 24 |
							(boost::uint64_t(data[7]) & 0xff) << 16 |
							(boost::uint64_t(data[8]) & 0xff) << 8 |
							(boost::uint64_t(data[9]) & 0xff);
		data_ptr = 10;
		break;
	default:
		/* leave it as it is */
		break;
	}

	std::cout << "Payload length is " << this->payload_len << std::endl;

	if ( true == this->mask )
	{
		this->masking_key[0] = data[data_ptr];
		this->masking_key[1] = data[data_ptr+1];
		this->masking_key[2] = data[data_ptr+2];
		this->masking_key[3] = data[data_ptr+3];


		data_ptr += 4;
	}

	/* our implementation will not require very large packets
	 * so we can assume we will not have any continuation frames
	 */
	if ( op_code_text == this->op_code )
	{
		char * ptr = data.data() + data_ptr;

		if (true == this->mask)
		{
			for (uint64_t i = 0; i < this->payload_len; i++)
			{
				this->bodytext += (*ptr) xor this->masking_key[i % 4];
				ptr++;
			}
		}
		else
		{
			for (uint64_t i = 0; i < this->payload_len; i++)
			{
				this->bodytext += *ptr;
				ptr++;
			}
		}
	}
}
