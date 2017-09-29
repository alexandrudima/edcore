/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#ifndef SRC_EDCORE_H_
#define SRC_EDCORE_H_

#include <memory>
#include <vector>

using namespace std;

namespace edcore
{

class String
{
  public:
    virtual void print(ostream &os) const = 0;
    virtual size_t getLen() const = 0;
    virtual void writeTo(uint16_t *dest) const = 0;
};

class BufferString : public String
{
  private:
    uint16_t *_data;
    size_t _len;

    size_t *_lineStarts;
    size_t _lineStartsCount;

    void _init(uint16_t *data, size_t len, size_t *lineStarts, size_t lineStartsCount);

  public:
    BufferString(uint16_t *data, size_t len);

    ~BufferString();

    size_t getLen() const;

    size_t getNewLineCount() const;

    bool getEndsWithCR() const;

    bool getStartsWithLF() const;

    const uint16_t *getData() const; // TODO

    const size_t *getLineStarts() const;

    void print(ostream &os) const;

    void writeTo(uint16_t *dest) const;
};

class BufferNode;

// class BufferCoordinate {
//     size_t offset;
//     BufferNode
// }

class BufferCursor
{
  public:
    size_t offset;

    BufferNode *node;
    size_t nodeStartOffset;

    BufferCursor()
        : BufferCursor(0, NULL, 0)
    {
    }

    BufferCursor(BufferCursor &src)
        : BufferCursor(src.offset, src.node, src.nodeStartOffset)
    {
    }

    BufferCursor(size_t offset, BufferNode *node, size_t nodeStartOffset)
    {
        this->offset = offset;
        this->node = node;
        this->nodeStartOffset = nodeStartOffset;
    }
};

class BufferNode
{
  private:
    shared_ptr<BufferString> _str;

    BufferNode *_leftChild;
    BufferNode *_rightChild;

    BufferNode *_parent;
    size_t _len;
    size_t _newLineCount;
    bool _endsWithCR;
    bool _startsWithLF;

    void _init(
        shared_ptr<BufferString> str,
        BufferNode *leftChild,
        BufferNode *rightChild,
        size_t len,
        size_t newLineCount,
        bool startsWithLF,
        bool endsWithCR);

  public:
    BufferNode(shared_ptr<BufferString> str);

    BufferNode(BufferNode *leftChild, BufferNode *rightChild);

    ~BufferNode();

    void print(ostream &os);

    void log(ostream &os, int indent);

    bool isLeaf() const;

    void setParent(BufferNode *parent);

    size_t getLen() const;

    size_t getNewLineCount() const;

    BufferNode *findPieceAtOffset(size_t &offset);

    BufferNode *firstLeaf();
    BufferNode *next();
    shared_ptr<String> getStrAt(size_t offset, size_t len);
    shared_ptr<String> _getStrAt(BufferNode *node, size_t offset, size_t len);
    BufferNode *findPieceAtLineIndex(size_t &lineIndex);
    size_t getLineLength(size_t lineNumber);
    size_t _getLineIndexLength(const size_t lineIndex, BufferNode *node, const size_t lineStartOffset);

    bool findOffset(size_t offset, BufferCursor &result);
    bool findLine(size_t lineNumber, BufferCursor &start, BufferCursor &end);


    bool _findLineStart(size_t &lineIndex, BufferCursor &result);
    void _findLineEnd(BufferNode *node, size_t nodeStartOffset, size_t innerLineIndex, BufferCursor &result);
    void extractString(BufferCursor start, size_t len, uint16_t *dest);
    // bool findLineStart(size_t lineNumber, BufferCursor &result);
    // void moveToLineEnd(BufferCursor &cursor);
    // bool findOffset(size_t offset, BufferCursor &result);
};

class Buffer
{
  private:
    BufferNode *root;

  public:
    Buffer(BufferNode *root);
    ~Buffer();
    size_t getLen() const;
    size_t getLineCount() const;
    shared_ptr<String> getStrAt(size_t offset, size_t len);
    size_t getLineLength(size_t lineNumber);
    void print(ostream &os);

    // BufferCursor& findOffset(size_t offset);
    bool findOffset(size_t offset, BufferCursor &result);
    bool findLine(size_t lineNumber, BufferCursor &start, BufferCursor &end);
    void extractString(BufferCursor start, size_t len, uint16_t *dest);
    // bool findLineStart(size_t lineNumber, BufferCursor &result);
    // void moveToLineEnd(BufferCursor &cursor);
};

class BufferBuilder
{
  private:
    vector<shared_ptr<BufferString>> _rawPieces;
    bool _hasPreviousChar;
    uint16_t _previousChar;

  public:
    BufferBuilder();
    void AcceptChunk(uint16_t *chunk, size_t chunkLen);
    void Finish();
    Buffer *Build();
};
}

#endif
