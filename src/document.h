#include <memory>
#include <string>
#include <vector>

struct node_t;
typedef std::shared_ptr<node_t> node_ptr;

enum node_type_e {
    node_type_original = 0,
    node_type_added
};

struct node_t {
    int start;
    int length;
    int lineBreaks;
    node_type_e type;
    std::vector<int> indices;
};

struct document_t {

    void initialize(std::string text);
    void insertAt(int index, std::string text);
    void deleteAt(int index, int length);
    std::string getTextAtLine(int line);
    std::string getText();
    std::string getTextAt(int index, int length);

    std::string* _buffers[2];
    std::string _newline;
    std::string original;
    std::string added;

    std::vector<node_ptr> nodes;

    int _findNodeFromIndex(int index, int& offset);
    void _deleteAt(int index);
    int _countLineBreaks(std::string str, std::vector<int>* indices = NULL);
    int _countNodeLineBreaks(node_ptr node);
};

typedef std::shared_ptr<document_t> document_ptr;
