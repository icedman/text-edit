#include <stdio.h>
#include <string>

#include "document.h"

int main()
{
    document_t doc;
    doc.initialize("the\nquick brown fox jumped over the lazy dog\n lazy dog\n lazy dog\n lazy dog-");
    printf(">%s\n", doc.getText().c_str());
    doc.insertAt(10, "yellow ");
    printf(">%s\n", doc.getText().c_str());
    doc.insertAt(17, "brown ");
    printf(">%s\n", doc.getText().c_str());
    doc.deleteAt(10, 5);
    doc.deleteAt(11, 6);
    printf(">%s\n", doc.getText().c_str());
    // printf(">%s\n",  doc.getTextAt(200, 10).c_str());
    printf(">>>%s\n", doc.getTextAtLine(0).c_str());
    printf(">>>%s\n", doc.getTextAtLine(1).c_str());
    printf(">>>%s\n", doc.getTextAtLine(4).c_str());
    return 0;
}
