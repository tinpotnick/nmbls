#include <iostream>
#include <boost/algorithm/string.hpp>
#include <list>
#include "simpletemplate.h"

/*******************************************************************************
Function: nmbls::simpletemplate::compile
Description: Compile simple template.
Date: 14.12.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::simpletemplate::compile( std::string simpletemplate )
{
  std::size_t tokenpos;
  this->startchunks.clear();
  this->chunks.clear();

  while ( simpletemplate.size() > 0 )
  {
    simpletemplatechunk nextchunk;
    nextchunk.chunktype = simpletemplatechunk::simpletemplatechunktype::chunktypetext;
    tokenpos = simpletemplate.find( "{{");
    if ( tokenpos == std::string::npos )
    {
      nextchunk.text = simpletemplate;
      this->chunks.push_back( nextchunk );
      break;
    }

    nextchunk.text = simpletemplate.substr( 0, tokenpos );
    this->chunks.push_back( nextchunk );

    // After the {{
    simpletemplate = simpletemplate.substr( tokenpos + 2, simpletemplate.size() );

    this->entertoken( simpletemplate );
  }
}

/*******************************************************************************
Function: nmbls::simpletemplate::entertoken
Description: Private function - part of compile.
Date: 14.12.2017
Author: Nick Knight
*******************************************************************************/
void nmbls::simpletemplate::entertoken( std::string &simpletemplate )
{
  std::size_t pos;
  pos = simpletemplate.find( "}}");
  bool isstartchunk = false;

  if ( pos == std::string::npos )
  {
    simpletemplate = "";
    return;
  }

  simpletemplatechunk nextchunk;
  nextchunk.chunktype = simpletemplatechunk::simpletemplatechunktype::chunktypetoken;
  nextchunk.text = simpletemplate.substr( 0, pos );
  simpletemplate = simpletemplate.substr( pos + 2, simpletemplate.size() );

  boost::trim_right( nextchunk.text );
  boost::trim_left( nextchunk.text );

  boost::split( nextchunk.command, nextchunk.text, boost::is_any_of( "\t " ) );

  std::string primarycommand = nextchunk.command[ 0 ];
  nextchunk.command.erase( nextchunk.command.begin() );

  if ( "for" == primarycommand )
  {
    nextchunk.chunktype = simpletemplatechunk::simpletemplatechunktype::chunktypefor;
    isstartchunk = true;

    this->chunks.push_back( nextchunk );
  }
  else if ( "if" == primarycommand )
  {
    nextchunk.chunktype = simpletemplatechunk::simpletemplatechunktype::chunktypeif;
    isstartchunk = true;

    this->chunks.push_back( nextchunk );
  }
  else if ( "end" == primarycommand )
  {
    std::list < simpletemplatechunklist::iterator >::iterator laststart = this->startchunks.end();
    --laststart;

    nextchunk.chunktype = simpletemplatechunk::simpletemplatechunktype::chunktypeend;
    nextchunk.pair = *laststart;

    this->startchunks.pop_back();
    this->chunks.push_back( nextchunk );

    simpletemplatechunklist::iterator newitem = this->chunks.end();
    --newitem;

    (*laststart)->pair = newitem;
  }
  else
  {
    this->chunks.push_back( nextchunk );
  }

  if ( true == isstartchunk )
  {
    simpletemplatechunklist::iterator laststart = this->chunks.end();
    --laststart;
    this->startchunks.push_back( laststart );
  }
}

/*******************************************************************************
Function: nmbls::simpletemplate::render
Description: Renders the compiled template into a string.
Date: 14.12.2017
Author: Nick Knight
*******************************************************************************/
std::string nmbls::simpletemplate::render( boost::property_tree::ptree &tree )
{
  std::string retval;
  simpletemplatechunklist::iterator it = this->chunks.begin();
  this->render( tree, it, retval );
  return retval;
}

/*******************************************************************************
Function: nmbls::simpletemplate::render
Description: Renders the compiled template into a string. Private recursive version.
Date: 15.12.2017
Author: Nick Knight
*******************************************************************************/
nmbls::simpletemplatechunklist::iterator nmbls::simpletemplate::render( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out )
{
  simpletemplatechunklist::iterator it;

  for ( it = start; it != this->chunks.end(); it++ )
  {
    if( simpletemplatechunk::simpletemplatechunktype::chunktypetext == it->chunktype )
    {
      out += it->text;
    }
    else if ( simpletemplatechunk::simpletemplatechunktype::chunktypetoken == it->chunktype )
    {
      try
      {
        out += tree.get< std::string >( it->text );
      }
      catch( const boost::property_tree::ptree_error &e )
      {
        // Silent is ok for this.
      }
    }
    else if ( simpletemplatechunk::simpletemplatechunktype::chunktypefor == it->chunktype )
    {
      it = this->enterfor( tree, it, out );
    }
    else if ( simpletemplatechunk::simpletemplatechunktype::chunktypeif == it->chunktype )
    {
      it = this->enterif( tree, it, out );
    }
    else if ( simpletemplatechunk::simpletemplatechunktype::chunktypeend == it->chunktype )
    {
      it++;
      return it;
    }
  }
  return it;
}

static void getptreeitemorkey( std::string &retval, std::string key, boost::property_tree::ptree &tree )
{
  try
  {
    retval = tree.get< std::string >( key );
    return;
  }
  catch( const boost::property_tree::ptree_error &e )
  {
    // Silent is ok.
  }
  retval = key;
}

static void getptreeitemorkey( double &retval, std::string key, boost::property_tree::ptree &tree )
{
  try
  {
    retval = tree.get< double >( key );
    return;
  }
  catch( const boost::property_tree::ptree_error &e )
  {
    // Silent is ok.
  }
  retval = std::atof( key.c_str() );
}

/*******************************************************************************
Function: nmbls::simpletemplate::enterfor
Description: Whilst rendering parse an if section, it will parse the section if
the if matches. Either side could expand from the property tree.
if i > 5
if i >= 5
if i < 5
if i <= 5
if i == 5
if i != 5
Date: 15.12.2017
Author: Nick Knight
*******************************************************************************/
nmbls::simpletemplatechunklist::iterator nmbls::simpletemplate::enterif( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out )
{
  std::string command = boost::algorithm::join( start->command, "" );
  std::vector < std::string > parts;
  boost::split( parts, command, boost::is_any_of( "!=<>" ) );
  simpletemplatechunklist::iterator it;

  std::string leftstr, rightstr, leftstrval, rightstrval;
  double leftstrd, rightstrd;

  leftstr = parts[ 0 ];
  rightstr = parts[ parts.size() - 1 ];

  simpletemplatechunklist::iterator endif = start->pair;
  start++;

  if ( std::string::npos != command.find( "==" ) )
  {
    getptreeitemorkey( leftstrval, leftstr, tree );
    getptreeitemorkey( rightstrval, rightstr, tree );

    if ( rightstrval == leftstrval )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }
  else if ( std::string::npos != command.find( "!=" ) )
  {
    getptreeitemorkey( leftstrval, leftstr, tree );
    getptreeitemorkey( rightstrval, rightstr, tree );

    if ( rightstrval == leftstrval )
    {
      it = endif;
      it++;
    }
    else
    {
      it = this->render( tree, start, out );
    }
  }
  else if ( std::string::npos != command.find( "<=" ) )
  {
    getptreeitemorkey( leftstrd, leftstr, tree );
    getptreeitemorkey( rightstrd, rightstr, tree );

    if ( leftstrd <= rightstrd )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }
  else if ( std::string::npos != command.find( ">=" ) )
  {
    getptreeitemorkey( leftstrd, leftstr, tree );
    getptreeitemorkey( rightstrd, rightstr, tree );

    if ( leftstrd >= rightstrd )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }
  else if ( std::string::npos != command.find( "<" ) )
  {
    getptreeitemorkey( leftstrd, leftstr, tree );
    getptreeitemorkey( rightstrd, rightstr, tree );

    if ( leftstrd < rightstrd )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }
  else if ( std::string::npos != command.find( ">" ) )
  {
    getptreeitemorkey( leftstrd, leftstr, tree );
    getptreeitemorkey( rightstrd, rightstr, tree );

    if ( leftstrd > rightstrd )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }
  else
  {
    getptreeitemorkey( leftstrval, leftstr, tree );
    if ( "true" == leftstrval || "1" == leftstrval
   )
    {
      it = this->render( tree, start, out );
    }
    else
    {
      it = endif;
      it++;
    }
  }

  return it;
}

/*******************************************************************************
Function: nmbls::simpletemplate::enterfor
Description: Whilst rendering parse a for section, it will bomb out after one
itteration if no end is found. For command format
for i = lowervariable:uppervariable
for i = lowervariable:increby:uppervariable
Date: 15.12.2017
Author: Nick Knight
*******************************************************************************/
nmbls::simpletemplatechunklist::iterator nmbls::simpletemplate::enterfor( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out )
{
  std::string command = boost::algorithm::join( start->command, "" );
  std::vector < std::string > parts;

  boost::split( parts, command, boost::is_any_of( "=:" ) );

  int incrby = 1;
  int endi = 0;
  if( 4 == parts.size() )
  {
    incrby = std::stoi( parts[ 2 ] );
    endi = std::stoi( parts[ 3 ] );
  }
  else
  {
    endi = std::stoi( parts[ 2 ] );
  }

  start++;
  simpletemplatechunklist::iterator it;
  for ( int starti = std::stoi( parts[ 1 ] ); starti <= endi; starti += incrby  )
  {
    tree.put( parts[ 0 ], starti );
    it = this->render( tree, start, out );
  }

  return it;
}
