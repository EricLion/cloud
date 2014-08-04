#include "xmlTool.h"

xmlNodePtr currentNode;

xmlNodePtr parseDoc(char *docname,xmlDocPtr doc) {

//    xmlDocPtr doc;
    xmlNodePtr cur;

    xmlKeepBlanksDefault(0);
//    doc = xmlParseFile(docname);
    doc = xmlReadFile(docname,"UTF-8",XML_PARSE_NOBLANKS);

    if (doc == NULL ) {
        fprintf(stderr,"Document not parsed successfully. \n");
        return;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
        fprintf(stderr,"empty document\n");
        xmlFreeDoc(doc);
        return;
    }
    printf("Root Node: %s\n", cur->name);
    return cur;
}

void printChildrenNames(xmlNodePtr cur) {
	xmlChar* type;
	xmlChar* value;
	xmlAttrPtr attrPtr;
    if (cur != NULL) {
        cur = cur->xmlChildrenNode;

        while (cur != NULL){
            printf("Current Node: %s\n", cur->name);
            if (xmlHasProp(cur,BAD_CAST "t"))
                {
            	attrPtr = cur->properties;
                }
            while (attrPtr != NULL)
            	{
            	 if (!xmlStrcmp(attrPtr->name, BAD_CAST "t"))
                 	 {
                   type = xmlGetProp(cur,BAD_CAST "t");
                   printf("type: %s\n", type);
                 	 }
            	 if (!xmlStrcmp(attrPtr->name, BAD_CAST "v"))
                     {
            		 value = xmlGetProp(cur,BAD_CAST "v");
            		 printf("value: %s\n", value);
                     }
                attrPtr = attrPtr->next;
            	}
            printChildrenNames(cur);
            cur = cur->next;
        }

        return;
    }else{
        fprintf(stderr, "ERROR: Null Node!");
        return;
    }
}

int main(int argc, char **argv) {

    char *docname;
    xmlDocPtr doc;

    if (argc <= 1) {
        printf("Usage: %s docname\n", argv[0]);
        return(0);
    }

    docname = argv[1];
    currentNode = parseDoc (docname, doc);
    //printf("Root Node: %s\n", currentNode->name);

    printChildrenNames(currentNode);
    xmlFreeDoc(doc);
    return (1);
}
