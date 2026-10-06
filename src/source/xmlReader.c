#include "../include/xmlReader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ERROR -0xFFFFFF

enum ErrorCodes{
    ERROR_UNEXPECTED_SYMBOL = 0xFFA000,
    ERROR_UNCLOSED_ELEMENT,
    ERROR_UNEXPECTED_CLOSING_TAG,
    ERROR_UNCLOSED_COMMENT,
    ERROR_EXPECTED_SYMBOL_NOT_FOUND,
    ERROR_EXPECTED_VALUE_NOT_FOUND,
    ERROR_UNCLOSED_VALUE,
    ERROR_EXPECTED_DECLARATION_END
};

typedef struct{
    char * token;
    int line, tokenLength;
} Token;

typedef struct XMLVALELEMENT{
    List elements;
    xmlElement * el;
    struct XMLVALELEMENT * parent;
    char firstTagClosed, secondTagClosed;
    int generation;
} xmlValElement;

typedef struct{
    char isElementName, 
        isOpeningTag,
        isInnerContent,
        isText,
        isComment,
        isValue,
        isDeclaration,
        isExclamationMark,
        returnToParent,
        valueStopChar,
        expectedSymbol;
    int generation;
} ParsingData;

typedef struct{
    char * buffer;
    size_t length;
} TextBuffer;

xmlAttribute * createAttribute(char * name, char * value, int nLength, int vLength, int type){
    xmlAttribute * o = malloc(sizeof(xmlAttribute));

    o->name = name;
    o->value = value;
    o->nameLength = nLength;
    o->valueLength = vLength;
    o->type = (char)type;

    return o;
}

void freeAttribute(void * attribute){
    xmlAttribute * attr = attribute;

    free(attr->name);
    free(attr->value);
    free(attr);
}

xmlElement * createElement(char * name, int nameLength, int type){
    xmlElement * o = malloc(sizeof(xmlElement));

    o->elements = LST_createList();
    o->attributes = LST_createList();
    o->name = name;
    o->nameLength = nameLength;
    o->type = type;

    return o;
}

void freeElement(void * element){
    xmlElement * el = element;

    for(int i = 0; i < el->elements.length; i++){
        xmlElement * child = LST_get(el->elements, i);
        freeElement(child);
    }

    LST_clear(&el->elements, 0);

    for(int i = 0; i < el->attributes.length; i++){
        xmlAttribute * attr = LST_get(el->attributes, i);
        freeAttribute(attr);
    }

    LST_clear(&el->attributes, 0);

    free(el->name);
    free(el);
}

Token * createToken(int line, int tokenLength){
    Token * o = malloc(sizeof(Token));

    o->token = malloc(tokenLength + 1);
    o->line = line;
    o->tokenLength = tokenLength;

    return o;
}

void freeToken(void * token){
    Token * t = token;

    free(t->token);
    free(t);
}

xmlValElement * createValElement(xmlElement * el, xmlValElement * parent){
    xmlValElement * o = malloc(sizeof(xmlValElement));

    o->elements = LST_createList();
    o->el = el;
    o->parent = parent;
    o->firstTagClosed = 0;
    o->secondTagClosed = 0;

    return o;
}

void freeValElement(void * valElement){
    xmlValElement * ve = valElement;

    for(int i = 0; i < ve->elements.length; i++){
        xmlValElement * child = LST_get(ve->elements, i);
        freeValElement(child);
    }
    
    LST_clear(&ve->elements, 0);

    free(ve);
}

void freeValElementWithContent(xmlValElement * ve){
    for(int i = 0; i < ve->elements.length; i++){
        xmlValElement * child = LST_get(ve->elements, i);
        freeValElementWithContent(child);
    }

    freeElement(ve->el);
    LST_clear(&ve->elements, 0);

    free(ve);
}

int isAnXmlFile(const char * path){
    int length = strlen(path);

    if(length < 5){
        return 0;
    }

    char extension[5];
    int start = length - 4;
    
    for(int i = 0; i < 5; i++){
        extension[i] = tolower(path[i + start]);
    }

    if(!strncmp(extension, ".xml", 5)){
        return 1;
    }
    else{
        return 0;
    }
}

char isWhiteSpace(char c){
    return (c == ' ' || c == '\n' || c == '\t');
}

char isNotWhiteSpace(char c){
    return (c != ' ' && c != '\n' && c != '\t');
}

char isNameChar(char c){
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-');
}

char isNameCharWithSigns(char c){
    return ((c > 32 && c < 127) && c != '<' && c != '>');
}

char isNameCharWithSignsEscape(char c){
    return (c > 32 && c < 127);
}

char isValueChar(char c, char valueStopChar){
    return (c > 32 && c < 127) && c != valueStopChar;
}

int lengthUntilWhitespace(FILE * file){
    char c;
    int l = 1;

    while(isNotWhiteSpace(c = getc(file)) && c != EOF) l++;

    return l;
}

char clearWhiteSpace(FILE * file, int * line){
    char c;
    while(isWhiteSpace(c = getc(file))){
        if(c == '\n') (int)(*line)++;
    }
    return c;
}

char getNonWhitepace(FILE * file, char * array, int length, char firstChar){
    char c;

    array[0] = firstChar;

    for(int i = 1; i < length; i++){
        array[i] = getc(file);
        c = array[i];
    }

    array[length] = '\0';

    return c;
}

List tokenize(FILE * file){
    List tokens = LST_createList();

    char c;
    fpos_t cursor;
    int line = 1;
    
    c = clearWhiteSpace(file, &line);
    fgetpos(file, &cursor);
    
    while(c != EOF){
        int length = lengthUntilWhitespace(file);
        fsetpos(file, &cursor);

        Token * token = createToken(line, length);
        getNonWhitepace(file, token->token, length, c);
        LST_add(&tokens, token, &freeToken);

        c = clearWhiteSpace(file, &line);
        fgetpos(file, &cursor);
    }

    return tokens;
}

void printUnexpectedSymbolError(Token * token, char symbol){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNEXPECTED_SYMBOL);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tUNEXPECTED SYMBOL: '%c'\n", symbol);
    printf("\tValidation failed\n\n");
}

void printUnclosedElementError(Token * token, xmlValElement * element){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNCLOSED_ELEMENT);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tELEMENT <%s> IN <%s> IS UNCLOSED\n", element->el->name, element->parent->el->name);
    printf("\tValidation failed\n\n");
}

void printUnexpectedClosingTagError(Token * token, char * tagName){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNEXPECTED_CLOSING_TAG);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tUNEXPECTED CLOSING TAG: </%s>\n", tagName);
    printf("\tValidation failed\n\n");
}

void printUnclosedCommentError(){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNCLOSED_COMMENT);
    printf("\tUNCLOSED COMMENT DETECTED\n");
    printf("\tValidation failed\n\n");
}

void printExpectedSymbolNotFoundError(Token * token, ParsingData parsingData){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNEXPECTED_SYMBOL);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tEXPECTED SYMBOL '%c' BUT NOT FOUND\n", parsingData.expectedSymbol);
    printf("\tValidation failed\n\n");
}

void printExpectedValueNotFoundError(Token * token){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_EXPECTED_VALUE_NOT_FOUND);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tEXPECTED VALUE: EXPECTED SYMBOL '\"' OR '\n'");
    printf("\tValidation failed\n\n");
}

void printUnclosedValueError(){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_UNCLOSED_VALUE);
    printf("\tUNCLOSED VALUE DETECTED\n");
    printf("\tValidation failed\n\n");
}

void printExpectedDeclarationEnd(Token * token){
    printf("XML reader [validation]: CODE 0x%x\n", ERROR_EXPECTED_DECLARATION_END);
    printf("\ton line: %d near: '%s'\n", token->line, token->token);
    printf("\tEXPECTED CLOSING SYMBOL '?' BUT NOT FOUND\n");
    printf("\tValidation failed\n\n");
}

char * rootName(){
    char * name = malloc(5);
    name[0] = 'r'; name[1] = 'o'; name[2] = 'o'; name[3] = 't'; name[4] = '\0';
    return name;
}

int getLengthWhenNameChar(char * token, int start){
    int i = start, l = 0;

    while(isNameChar(token[i]) && token[i] != '\0'){
        i++;
        l++;
    }

    return l;
}

int getLengthWhenNameSignsChar(char * token, int start, int escapeSequence){
    int i = start, l = 0;

    if(escapeSequence){
        while(isNameCharWithSignsEscape(token[i]) && token[i] != '\0' && l < 1){
            i++;
            l++;
        }
    }
    else{
        while(isNameCharWithSigns(token[i]) && token[i] != '\0'){
            i++;
            l++;
        }
    }

    return l;
}

int getLengthWhenValueChar(char * token, int start, char valueStopChar){
    int i = start, l = 0;

    while(isValueChar(token[i], valueStopChar) && token[i] != '\0'){
        i++;
        l++;
    }

    return l;
}

//1
int openElement(ParsingData * parsingData, xmlValElement ** cursor){
    xmlValElement * newElement = createValElement(createElement(NULL, 0, XML_ELEMENT), *cursor);
    xmlValElement * _cursor = *cursor;
    newElement->generation = ++parsingData->generation;
    LST_add(&_cursor->elements, newElement, &freeValElement);
    
    *cursor = newElement;

    parsingData->isOpeningTag = 1;
    parsingData->isInnerContent = 0;
    parsingData->isElementName = 1;

    return 1;
}

//2
int nameElement(ParsingData * parsingData, xmlValElement * cursor, char * tokenVal, int index){
    int nameLength = getLengthWhenNameChar(tokenVal, index);
    char * name = malloc(nameLength + 1);
    strncpy(name, &tokenVal[index], nameLength);
    name[nameLength] = '\0';

    int oldNameLength = 0;
    
    if(tokenVal[index + nameLength] == ':'){
        oldNameLength = nameLength + 1;
        int newIndex = index + oldNameLength;
        free(name);

        nameLength = getLengthWhenNameChar(tokenVal, newIndex);
        name = malloc(nameLength + 1);
        strncpy(name, &tokenVal[newIndex], nameLength);
        name[nameLength] = '\0';
    }

    cursor->el->name = name;
    cursor->el->nameLength = nameLength;

    parsingData->isElementName = 0;

    return nameLength + oldNameLength;
}

//3
int endTag(ParsingData * parsingData, xmlValElement ** cursor){
    xmlValElement * _cursor = *cursor;

    if(parsingData->isOpeningTag){
        _cursor->firstTagClosed = 1;
    }
    else{
        _cursor->secondTagClosed = 1;
        parsingData->returnToParent = 1;
        *cursor = _cursor->parent;
    }

    parsingData->isInnerContent = 1;
    parsingData->isOpeningTag = 0;
    parsingData->generation--;

    return 1;
}

//4
int closeElement(ParsingData * parsingData){
    parsingData->returnToParent = 1;
    parsingData->isInnerContent = 0;
    parsingData->isOpeningTag = 0;

    return 2;
}

int returnToParent(ParsingData * parsingData, xmlValElement ** cursor, char * tokenVal, int index, int * errorCode){
    int nameLength = getLengthWhenNameChar(tokenVal, index);
    char * name = malloc(nameLength + 1);
    strncpy(name, &tokenVal[index], nameLength);
    name[nameLength] = '\0';

    int oldNameLength = 0;
    
    if(tokenVal[index + nameLength] == ':'){
        oldNameLength = nameLength + 1;
        int newIndex = index + oldNameLength;
        free(name);

        nameLength = getLengthWhenNameChar(tokenVal, newIndex);
        name = malloc(nameLength + 1);
        strncpy(name, &tokenVal[newIndex], nameLength);
        name[nameLength] = '\0';
    }

    xmlValElement * _cursor = *cursor;

    if(strcmp(_cursor->el->name, name)){
        if(!_cursor->secondTagClosed){
            *errorCode = ERROR_UNCLOSED_ELEMENT;
        }
        else{
            *errorCode = ERROR_UNEXPECTED_CLOSING_TAG;
            free((*cursor)->el->name);
            (*cursor)->el->name = name;
        }
    }

    return nameLength + oldNameLength;
}

//5
int expandBuffer(ParsingData * parsingData, TextBuffer * buffer, char * tokenVal, int index, char escapeSequence){
    parsingData->isText = 1;
    
    int textLength = getLengthWhenNameSignsChar(tokenVal, index, escapeSequence);
    size_t bufferLength;
    char * newBuffer;

    char lastChar = buffer->length > 0 ? buffer->buffer[buffer->length - 1] : 0;
    
    if(buffer->length == 0){
        bufferLength = textLength;
        
        newBuffer = malloc(bufferLength + 1);
        strncpy(newBuffer, &tokenVal[index], bufferLength);
    }
    else if(escapeSequence){
        bufferLength = textLength + buffer->length - 1;
        
        newBuffer = malloc(bufferLength + 1);
        
        strncpy(newBuffer, buffer->buffer, buffer->length - 1);
        strncpy(&newBuffer[buffer->length - 1], &tokenVal[index], textLength);
    }
    else if((lastChar == '>' || lastChar == '<') && index > 0){
        bufferLength = textLength + buffer->length;
        
        newBuffer = malloc(bufferLength + 1);
        
        strncpy(newBuffer, buffer->buffer, buffer->length);
        strncpy(&newBuffer[buffer->length], &tokenVal[index], textLength);
    }
    else{
        bufferLength = textLength + buffer->length + 1;
        
        newBuffer = malloc(bufferLength + 1);
        
        strncpy(newBuffer, buffer->buffer, buffer->length);
        newBuffer[buffer->length] = ' ';
        strncpy(&newBuffer[buffer->length + 1], &tokenVal[index], textLength);
    }
    
    newBuffer[bufferLength] = '\0';
    
    if(buffer->buffer != NULL) free(buffer->buffer);
    buffer->buffer = newBuffer;
    buffer->length = bufferLength;

    return textLength;
}

void terminateBuffer(ParsingData * parsingData, xmlValElement * cursor, TextBuffer * textBuffer){
    parsingData->isText = 0;

    xmlValElement * newElement = createValElement(createElement(textBuffer->buffer, textBuffer->length, XML_TEXT), cursor);
    newElement->firstTagClosed = 1;
    newElement->secondTagClosed = 1;
    LST_add(&cursor->elements, newElement, &freeValElement);

    textBuffer->buffer = NULL;
    textBuffer->length = 0;
}

//3 + 4
int standaloneClose(ParsingData * parsingData, xmlValElement * cursor){
    parsingData->returnToParent = 1;
    parsingData->isInnerContent = 0;
    parsingData->isOpeningTag = 0;
    cursor->firstTagClosed = 1;

    return 1;
}

//6, 7, 8
int attributeName(ParsingData * parsingData, xmlValElement * cursor, char * tokenVal, int index, int * errorCode){
    int attrNameLength = getLengthWhenNameChar(tokenVal, index);
    char * attrName = malloc(attrNameLength + 1);
    strncpy(attrName, &tokenVal[index], attrNameLength);
    attrName[attrNameLength] = '\0';

    int oldAttrNameLength = 0;
    int type = XML_ATTRIBUTE;
    
    if(!strcmp(attrName, "xmlns") && tokenVal[index + attrNameLength] == ':'){
        oldAttrNameLength = attrNameLength + 1;
        int newIndex = index + oldAttrNameLength;
        oldAttrNameLength = getLengthWhenNameChar(tokenVal, newIndex);

        type = XML_NSDECLARATION;
    }
    if(tokenVal[index + attrNameLength] == ':'){
        oldAttrNameLength = attrNameLength + 1;
        int newIndex = index + oldAttrNameLength;
        free(attrName);

        attrNameLength = getLengthWhenNameChar(tokenVal, newIndex);
        attrName = malloc(attrNameLength + 1);
        strncpy(attrName, &tokenVal[newIndex], attrNameLength);
        attrName[attrNameLength] = '\0';
    }

    int afterName = index + attrNameLength + oldAttrNameLength;

    if(attrNameLength == 0 && (tokenVal[afterName] == '=' || tokenVal[afterName] == '\'' || tokenVal[afterName] == '\"')){
        free(attrName);
        *errorCode = ERROR_UNEXPECTED_SYMBOL;

        return 0;
    }

    char * val = NULL;
    int indexOffset = 2;
    
    if(tokenVal[afterName] == '>' || tokenVal[afterName] == '\0'){
        val = malloc(5);
        val[0] = 't'; val[1] = 'r'; val[2] = 'u'; val[3] = 'e'; val[4] = '\0';
        indexOffset = 0;
    }
    else if(tokenVal[afterName] != '='){
        free(attrName);
        *errorCode = ERROR_EXPECTED_SYMBOL_NOT_FOUND;
        parsingData->expectedSymbol = '=';

        return 0;
    }

    afterName++;

    if((tokenVal[afterName] != '\'' && tokenVal[afterName] != '"') && !val){
        free(attrName);
        *errorCode = ERROR_EXPECTED_VALUE_NOT_FOUND;

        return 0;
    }

    if(!val){
        parsingData->isValue = 1;
        parsingData->valueStopChar = tokenVal[afterName];
    }

    xmlAttribute * attribute = createAttribute(attrName, val, attrNameLength, 0, type);
    LST_add(&cursor->el->attributes, attribute, &freeAttribute);

    return attrNameLength + indexOffset + oldAttrNameLength;
}

//9, 10
int attributeValue(ParsingData * parsingData, xmlValElement * cursor, TextBuffer * buffer, char * tokenVal, int index){
    int textLength = getLengthWhenValueChar(tokenVal, index, parsingData->valueStopChar);
    size_t bufferLength;
    char * newBuffer;

    if(buffer->length == 0){
        bufferLength = textLength;
        
        newBuffer = malloc(bufferLength + 1);
        strncpy(newBuffer, &tokenVal[index], bufferLength);
    }
    else{
        bufferLength = textLength + buffer->length + 1;
        
        newBuffer = malloc(bufferLength + 1);
        
        strncpy(newBuffer, buffer->buffer, buffer->length);
        newBuffer[buffer->length] = ' ';
        strncpy(&newBuffer[buffer->length + 1], &tokenVal[index], textLength);
    }

    newBuffer[bufferLength] = '\0';
    
    if(buffer->buffer != NULL) free(buffer->buffer);
    buffer->buffer = newBuffer;
    buffer->length = bufferLength;

    int afterValue = index + textLength;

    if(tokenVal[afterValue] == parsingData->valueStopChar){
        parsingData->valueStopChar = 0;
        parsingData->isValue = 0;

        xmlAttribute * attr = LST_get(cursor->el->attributes, cursor->el->attributes.length - 1);
        attr->value = buffer->buffer;
        attr->valueLength = buffer->length;

        buffer->buffer = NULL;
        buffer->length = 0;

        return textLength + 1;
    }
    else{
        return textLength;
    }
}

int openDeclaration(ParsingData * parsingData, xmlValElement ** cursor){
    xmlValElement * newElement = createValElement(createElement(NULL, 0, XML_DECLARATION), *cursor);
    xmlValElement * _cursor = *cursor;
    newElement->generation = ++parsingData->generation;
    LST_add(&_cursor->elements, newElement, &freeValElement);
    
    *cursor = newElement;

    parsingData->isOpeningTag = 1;
    parsingData->isInnerContent = 0;
    parsingData->isElementName = 1;
    parsingData->isDeclaration = 1;

    return 2;
}

int closeDeclaration(ParsingData * parsingData, xmlValElement * cursor){
    parsingData->isDeclaration = 0;
    parsingData->returnToParent = 1;
    parsingData->isInnerContent = 0;
    parsingData->isOpeningTag = 0;
    cursor->firstTagClosed = 1;
    return 1;
}

int openExclamationMark(ParsingData * parsingData, xmlValElement ** cursor){
    xmlValElement * newElement = createValElement(createElement(NULL, 0, XML_EXCLAMATIONMARK), *cursor);
    xmlValElement * _cursor = *cursor;
    newElement->generation = ++parsingData->generation;
    LST_add(&_cursor->elements, newElement, &freeValElement);
    
    *cursor = newElement;

    parsingData->isOpeningTag = 1;
    parsingData->isInnerContent = 0;
    parsingData->isElementName = 1;
    parsingData->isExclamationMark = 1;

    return 2;
}

int closeExclamationMark(ParsingData * parsingData, xmlValElement ** cursor){
    parsingData->isExclamationMark = 0;
    parsingData->returnToParent = 0;
    parsingData->isInnerContent = 0;
    parsingData->isOpeningTag = 0;
    ((xmlValElement *)*cursor)->firstTagClosed = 1;

    return 0;
}

xmlValElement * validate(List tokens){
    xmlValElement * result = createValElement(createElement(rootName(), 5, XML_ROOT), NULL);
    result->generation = 0;
    xmlValElement * cursor = result;

    ParsingData parsingData;
    TextBuffer textBuffer = (TextBuffer){NULL, 0};

    int errorCode = 0;

    Token * lastToken = NULL;

    parsingData.generation = 0;
    parsingData.isComment = 0;
    parsingData.returnToParent = 0;

    for(int i = 0; i < tokens.length; i++){
        Token * token = LST_get(tokens, i);

        char * tokenVal = token->token;
        int index = 0;

        while(tokenVal[index] != '\0'){
            if(parsingData.isComment){
                if(!strncmp(&tokenVal[index], "-->", 3)){
                    parsingData.isComment = 0;
                    index += 3;
                    continue;
                }
                else{
                    index++;
                    continue;
                }
            }

            switch(tokenVal[index]){
                case '<':
                    if(parsingData.isText){
                        if(index > 0 && tokenVal[index - 1] == '\\'){
                            index += expandBuffer(&parsingData, &textBuffer, tokenVal, index, 1);
                            break;
                        }
                        else{
                            terminateBuffer(&parsingData, cursor, &textBuffer);
                        }
                    }

                    switch(tokenVal[index + 1]){
                        case '/':
                            index += closeElement(&parsingData);
                        break;

                        case '!':
                            if(!strncmp(&tokenVal[index + 1], "!--", 3)){
                                parsingData.isComment = 1;
                                index += 4;
                            }
                            else{
                                index += openExclamationMark(&parsingData, &cursor);
                            }
                        break;

                        case '?':
                            index += openDeclaration(&parsingData, &cursor);
                        break;

                        default:
                            index += openElement(&parsingData, &cursor);
                        break;
                    }
                break;

                case '>':
                    if(parsingData.isInnerContent){
                        if(index > 0 && tokenVal[index - 1] == '\\'){
                            index += expandBuffer(&parsingData, &textBuffer, tokenVal, index, 1);
                        }
                        else{
                            errorCode = ERROR_UNEXPECTED_SYMBOL;
                        }

                        break;
                    }

                    if(parsingData.isDeclaration){
                        errorCode = ERROR_EXPECTED_DECLARATION_END;
                    }

                    if(parsingData.isExclamationMark){
                        index += closeExclamationMark(&parsingData, &cursor);
                    }

                    if(!parsingData.isElementName || !parsingData.returnToParent){
                        index += endTag(&parsingData, &cursor);
                    }
                    else{
                        errorCode = ERROR_UNEXPECTED_SYMBOL;
                    }
                break;

                case '/':
                    if(parsingData.isInnerContent){
                        index += expandBuffer(&parsingData, &textBuffer, tokenVal, index, 0);
                        break;
                    }

                    if(parsingData.isOpeningTag){
                        index += standaloneClose(&parsingData, cursor);
                    }
                    else{
                        errorCode = ERROR_UNEXPECTED_SYMBOL;
                    }
                break;

                case '?':
                    if(parsingData.isDeclaration){
                        index += closeDeclaration(&parsingData, cursor);
                    }
                    else{
                        errorCode = ERROR_UNEXPECTED_SYMBOL;
                    }
                break;

                default:
                    if(parsingData.isInnerContent){
                        index += expandBuffer(&parsingData, &textBuffer, tokenVal, index, 0);
                    }
                    else if(parsingData.isElementName){
                        index += nameElement(&parsingData, cursor, tokenVal, index);
                    }
                    else if(parsingData.isValue){
                        index += attributeValue(&parsingData, cursor, &textBuffer, tokenVal, index);
                    }
                    else if(parsingData.isOpeningTag){
                        index += attributeName(&parsingData, cursor, tokenVal, index, &errorCode);
                    }
                    else if(parsingData.returnToParent){
                        index += returnToParent(&parsingData, &cursor, tokenVal, index, &errorCode);
                    }
                    else{
                        errorCode = ERROR_UNEXPECTED_SYMBOL;
                    }
                break;
            }

            if(errorCode != 0){
                switch(errorCode){
                    case ERROR_UNEXPECTED_SYMBOL: printUnexpectedSymbolError(token, tokenVal[index]); break;
                    case ERROR_UNCLOSED_ELEMENT: printUnclosedElementError(token, cursor); break;
                    case ERROR_UNEXPECTED_CLOSING_TAG: printUnexpectedClosingTagError(token, cursor->el->name); break;
                    case ERROR_EXPECTED_SYMBOL_NOT_FOUND: printExpectedSymbolNotFoundError(token, parsingData); break;
                    case ERROR_EXPECTED_VALUE_NOT_FOUND: printExpectedValueNotFoundError(token); break;
                    case ERROR_EXPECTED_DECLARATION_END: printExpectedDeclarationEnd(token); break;
                }

                break;
            }
        }

        if(errorCode != 0) break;

        if(i == tokens.length - 1){
            xmlValElement * last = LST_get(result->elements, result->elements.length - 1);

            if(!last->firstTagClosed || !last->secondTagClosed){
                errorCode = ERROR_UNCLOSED_ELEMENT;
                printUnclosedElementError(token, last);
            }

            if(parsingData.isComment){
                errorCode = ERROR_UNCLOSED_COMMENT;
                printUnclosedCommentError();
            }

            if(parsingData.valueStopChar != 0){
                errorCode = ERROR_UNCLOSED_VALUE;
                printUnclosedValueError();
            }
        }
    }

    result->firstTagClosed = 1;
    result->secondTagClosed = 1;

    if(errorCode != 0){
        if(textBuffer.buffer) free(textBuffer.buffer);
        freeValElementWithContent(result);

        result = NULL;
    }

    return result;
}

xmlElement * translate(xmlValElement * result){
    xmlElement * el = result->el;
    
    for(int i = 0; i < result->elements.length; i++){
        xmlValElement * valChild = LST_get(result->elements, i);
        xmlElement * child = translate(valChild);
        LST_add(&el->elements, child, &freeElement);
    }
    
    return el;
}

xmlElement * xmlRead(FILE * file){
    List tokens = tokenize(file);

    xmlValElement * validationResult = validate(tokens);
    if(!validationResult){
        printf("XML reader (read()): Failure during validation.\n");
        LST_clear(&tokens, 1);
        return NULL;
    }

    LST_clear(&tokens, 1);

    xmlElement * result = translate(validationResult);
    freeValElement(validationResult);
    
    return result;
}

xmlElement * XML_read(const char * path){
    if(!path){
        printf("XML reader (read()): Path cannot be NULL.\n");
        return NULL;
    }

    FILE * file = fopen(path, "r");

    if(!file){
        printf("XML reader (read()): File not found.\n");
        return NULL;
    }

    if(!isAnXmlFile(path)){
        printf("XML reader (read()): That is not an XML file.\n");
        fclose(file);
        return NULL;
    }

    xmlElement * el = xmlRead(file);

    fclose(file);
    return el;
}

void XML_free(xmlElement * element){
    if(!element){
        printf("XML reader (free()): Element cannot be NULL.\n");
        return;
    }

    freeElement(element);
}