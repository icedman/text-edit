#include "document.h"
#include <string>

void document_t::initialize(std::string text)
{
    _buffers[0] = &original;
    _buffers[1] = &added;

    // get new line
    _newline = "\n";

    original = text;
    added = "";
    node_ptr node = std::make_shared<node_t>();
    node->start = 0;
    node->length = text.size();
    node->type = node_type_original;
    _countNodeLineBreaks(node);
    nodes.push_back(node);
}

int document_t::_findNodeFromIndex(int index, int& offset)
{
    index++;
    int idx = 0;
    for (auto n : nodes) {
        if (n->length == 0)
            continue;
        index -= n->length;
        if (index <= 0)
            return idx;
        offset += n->length;
        idx++;
    }
    return 0;
}

void document_t::insertAt(int index, std::string text)
{
    int offset = 0;

    int idx = _findNodeFromIndex(index, offset);
    int first_start = 0;
    int first_length = 0;
    index -= offset;

    printf(">%d\n", offset);

    // original - split first
    node_ptr first = nodes[idx];
    first_start = first->start;
    first_length = first->length;
    first->length = index;
    _countNodeLineBreaks(first);

    // added
    {
        auto pos = nodes.begin() + (idx + 1);
        node_ptr add = std::make_shared<node_t>();
        add->start = added.size();
        add->length = text.size();
        add->type = node_type_added;
        _countNodeLineBreaks(add);
        nodes.insert(pos, add);
        added += text;
    }

    // original - split second
    {
        auto pos = nodes.begin() + (idx + 2);
        node_ptr second;
        second = std::make_shared<node_t>();
        second->start = first_start + index;
        second->length = first_length - index;
        second->type = node_type_original;
        _countNodeLineBreaks(second);
        nodes.insert(pos, second);

        // shouldn't this be kept around?
        if (first->length == 0) {
            auto pos = nodes.begin() + idx;
            nodes.erase(pos);
        }
    }
}

void document_t::deleteAt(int index, int length)
{
    for (int i = 0; i < length; i++) {
        _deleteAt(index);
    }
}

void document_t::_deleteAt(int index)
{
    int offset = 0;

    int idx = _findNodeFromIndex(index, offset);
    int first_start = 0;
    int first_length = 0;
    index -= offset;

    // original - split first
    node_ptr first = nodes[idx];
    first_start = first->start;
    first_length = first->length;

    if (index == 0) {
        if (first->length > 0) {
            first->start++;
            first->length--;
        }
        _countNodeLineBreaks(first);
        return;
    }

    first->length = index;

    // original - split second
    {
        auto pos = nodes.begin() + (idx + 1);
        node_ptr second = std::make_shared<node_t>();
        second->start = first_start + first->length + 1;
        second->length = first_length - index - 1;
        second->type = first->type;
        _countNodeLineBreaks(second);
        nodes.insert(pos, second);
    }
}

int document_t::getIndexForLine(int line)
{
    if (nodes.size() == 1) {
        // access from cache
        node_ptr n = nodes[0];
        return n->indices[line];
    }

    int l = 0;
    int skippedLines = 0;
    int skippedLength = 0;
    std::vector<node_ptr> _nodes;
    std::string out;
    for (auto n : nodes) {
        l += n->lineBreaks;
        if (l < line) {
            skippedLines += n->lineBreaks;
            skippedLength += n->length;
            continue;
        }
        _nodes.push_back(n);
        if (l - 1 > line) {
            break;
        }
    }

    for (auto n : _nodes) {
        std::string* s = _buffers[n->type];
        std::string sub = s->substr(n->start, n->length);
        out += sub;
    }

    std::vector<int> indices;
    indices.push_back(0);
    _countLineBreaks(out, &indices);
    line -= skippedLines;
    return skippedLength + indices[line];
}

std::string document_t::getTextAtLine(int line)
{
    if (nodes.size() == 1) {
        // access from cache
        node_ptr n = nodes[0];
        // for(auto i : n->indices) {
        // printf("i:%d\n", i);
        // }
        std::string* s = _buffers[n->type];
        std::string sub = s->substr(n->start, n->length);
        return sub.substr(n->indices[line], n->indices[line + 1] - n->indices[line]);
    }

#if 0
    std::string out = getText();
    std::vector<int> indices;
    indices.push_back(0);
    _countLineBreaks(out, &indices);
    // for(auto i : indices) {
         // printf("i:%d\n", i);
    // }
    out = out.substr(indices[line], indices[line+1] - indices[line]);
    // printf("len:%d\n", out.size());
#else
    int l = 0;
    int skippedLines = 0;
    std::vector<node_ptr> _nodes;
    std::string out;
    for (auto n : nodes) {
        l += n->lineBreaks;
        if (l < line) {
            skippedLines += n->lineBreaks;
            continue;
        }
        _nodes.push_back(n);
        if (l - 1 > line) {
            break;
        }
    }

    for (auto n : _nodes) {
        std::string* s = _buffers[n->type];
        std::string sub = s->substr(n->start, n->length);
        out += sub;
    }

    std::vector<int> indices;
    indices.push_back(0);
    _countLineBreaks(out, &indices);
    // for(auto i : indices) {
    // printf("i:%d\n", i);
    // }
    line -= skippedLines;
    out = out.substr(indices[line], indices[line + 1] - indices[line]);
#endif
    return out;
}

std::string document_t::getText()
{
    std::string out;
    for (auto n : nodes) {
        std::string* s = _buffers[n->type];
        std::string sub = s->substr(n->start, n->length);
        // printf(">br:%d start:%d end:%d %s\n", n->lineBreaks, n->start, n->length, sub.c_str());
        out += sub;
        // out += "|";
    }
    return out;
}

std::string document_t::getTextAt(int index, int length)
{
    int start = -1;
    int offset = 0;
    std::string out;
    for (auto n : nodes) {
        if (offset + n->length < index)
            continue;
        std::string* s = _buffers[n->type];
        std::string sub = s->substr(n->start, n->length);
        // printf(">%d %d %d %s\n", n->type, n->start, n->length, sub.c_str());
        // out += "|";
        if (start == -1) {
            start = index - offset;
        }
        out += sub;
        offset += n->length;
        if (offset > index + length)
            break;
    }
    if (start == -1 || start > out.size())
        return "";
    out = out.substr(start, length);
    return out;
}

int document_t::_countLineBreaks(std::string str, std::vector<int>* indices)
{
    int idx = 0;
    int count = 0;
    do {
        std::string::size_type loc = str.find(_newline, idx + 1);
        if (loc == std::string::npos)
            break;
        idx = loc;
        if (indices != NULL) {
            indices->push_back(loc + 1);
        }
        count++;
    } while (true);
    return count;
}

int document_t::_countNodeLineBreaks(node_ptr node)
{
    std::string* s = _buffers[node->type];
    std::string sub = s->substr(node->start, node->length);
    node->indices.clear();
    node->indices.push_back(0);
    node->lineBreaks = _countLineBreaks(sub, &node->indices);
    return node->lineBreaks;
}
