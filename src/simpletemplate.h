
#include <string>
#include <list>
#include <vector>

#include <boost/property_tree/ptree.hpp>


namespace nmbls
{
  class simpletemplatechunk
  {
  public:
    enum simpletemplatechunktype { chunktypetext, chunktypetoken, chunktypefor, chunktypeif, chunktypeend };
    std::string text;
    std::vector< std::string > command;
    simpletemplatechunktype chunktype;
  };

  typedef std::list< simpletemplatechunk > simpletemplatechunklist;

  class simpletemplate
  {
  private:
    simpletemplatechunklist chunks;

    void entertoken( std::string &simpletemplate );
    simpletemplatechunklist::iterator enterfor( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out );
    simpletemplatechunklist::iterator enterif( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out );
    simpletemplatechunklist::iterator render( boost::property_tree::ptree &tree, simpletemplatechunklist::iterator start, std::string &out );

  public:
    inline simpletemplate() {}
    inline simpletemplate( std::string simpletemplate ) { this->compile( simpletemplate ); }
    void compile( std::string simpletemplate );
    std::string render( boost::property_tree::ptree &tree );

  };
}
