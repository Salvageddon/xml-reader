#pragma once

#include <salvagames/list.h>

enum xmlElementTypes{
    XML_ELEMENT = 0xA0000, //Normal XML element
    XML_ROOT, //XML root - that's this document
    XML_TEXT, //XML text - an inner text
    XML_DECLARATION, //Self explanatory
    XML_EXCLAMATIONMARK //For example <!DOCTYPE html> will be this type
};

enum xmlAttributeTypes{
    XML_ATTRIBUTE = 0xB0000, //Every attribute
    XML_NSDECLARATION //a xmlns attribute
};

typedef struct{
    List elements; //child elements list
    List attributes; //attribute list
    char * name; //element name
    int nameLength, //yes
        type; //type specified in xmlElementTypes enum
} xmlElement;

typedef struct{
    char * name, //name of the attribute 
        * value; //value of the attribute
    int nameLength, //right
        valueLength, //what do you think it it? :3
        type; //type specified in xmlAttributeTypes
} xmlAttribute;

/*
    Read XML file. Must be freed by XML_free();
    \param path path to a xml file
    \returns all xmlElements from the file, with the document as root
*/
xmlElement * XML_read(const char * path);

/*
    Free the result of XML_read();
    \param element result to be freed
*/
void XML_free(xmlElement * element);