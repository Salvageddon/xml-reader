#include "../include/xmlReader.h"
#include <stdio.h>

void printElementData(xmlElement * el, int generation){
    for(int i = 0; i < generation; i++){
        printf("  ");
    }

    printf("%s (%d) [%d] <%d> {", el->name, el->type, el->elements.length, el->attributes.length);

    for(int i = 0; i < el->attributes.length; i++){
        printf("\n");

        for(int j = 0; j < generation; j++){
            printf("  ");
        }

        xmlAttribute * attr = LST_get(el->attributes, i);

        printf("- %s => '%s',", attr->name, attr->value);
    }

    printf("}:\n");

    for(int i = 0; i < el->elements.length; i++){
        xmlElement * child = LST_get(el->elements, i);
        printElementData(child, generation + 1);
    }
}

int main(){
    List list = LST_createList();
    xmlElement * el = XML_read("src/pub/example.xml");

    if(el){
        printf("\nAmaze amaze amaze!!!\n\n");
        printElementData(el, 0);
    }
    else{
        printf("\nNot as good. What think eerth??\n");
    }

    XML_free(el);

    return 0;
}