#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstring>

#include <expat.h>

#include "XMLLoader.hh"
#include "ITSModelXMLLoader.hh"
#include "CompositeXMLLoader.hh"


void ITSModelXMLLoader::loadTypes (void * data, const XML_Char* Elt, const XML_Char** Attr)
{

  std::string name, formalism, format, path;
  std::stringstream s;
  
  its::ITSModel *model = (its::ITSModel *) data;
  
  // switch on element type :
  if (! strcmp(Elt,"type")) {
    for(int i=0; Attr[i]; i+=2) {
      if(!strcmp(Attr[i],"name")) {
	name = Attr[i+1];
      } else if (!strcmp(Attr[i],"formalism")) {
	formalism = Attr[i+1];
      } else if (!strcmp(Attr[i],"format")) {
	format = Attr[i+1];
      } else if (!strcmp(Attr[i],"path")) {
	path = Attr[i+1];
      }
    }
    // Decide which parser to invoke
    if ( format == "Romeo" && formalism == "TimePetriNet" ) {
      its::TPNet * pnet = XMLLoader::loadXML(path);
      model->declareType(*pnet);
    } else if ( format == "Composite" && formalism == "ITSComposite" ) {
      its::Composite * tcomp = CompositeXMLLoader::loadXML(path,*model,true);
      model->declareType(*tcomp);
    }
  }
}

void ITSModelXMLLoader::loadScenario (void * data, const XML_Char* Elt, const XML_Char** Attr)
{
  its::ITSModel *model = (its::ITSModel *) data;

  std::string type,state;
  // switch on element type :
  if (! strcmp(Elt,"main")) {
    for(int i=0; Attr[i]; i+=2) {
      if(!strcmp(Attr[i],"type")) {
	type = Attr[i+1];
      } else if (!strcmp(Attr[i],"state")) {
	state = Attr[i+1];
      }
    }
    model->setInstance (type,"main");
    model->setInstanceState("init");
  }
	
}


void ITSModelXMLLoader::loadXML(const std::string & filename, its::ITSModel & model) 
{
  char *Buffer = NULL;
  

  // Load the file in a buffer    
  int length;
  std::ifstream file;
  file.open(filename.c_str(),std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Error while opening: " << filename << std::endl;
    exit(1);
  }
  file.seekg(0,std::ios::end);
  length = file.tellg();
  file.seekg(0, std::ios::beg);
  Buffer = new char[length+1];
  file.read(Buffer, length);
  file.close();
	
  // Parsing types
  XML_Parser p = XML_ParserCreate(NULL);
  XML_SetUserData(p, &model);
  XML_SetElementHandler(p, ITSModelXMLLoader::loadTypes, NULL);
  XML_Parse(p, Buffer, strlen(Buffer), 1);
  XML_ParserFree(p);
	
  // Parsing arcs
  p = XML_ParserCreate(NULL);
  XML_SetUserData(p, &model);
  XML_SetElementHandler(p, ITSModelXMLLoader::loadScenario, NULL);
  XML_Parse(p, Buffer, strlen(Buffer), 1);
  XML_ParserFree(p);
	
  delete [] Buffer;
    
}
