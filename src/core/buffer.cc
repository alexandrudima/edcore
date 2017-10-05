/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "buffer.h"

#include <iostream>
#include <assert.h>
#include <cstring>

#define PARENT(i) (i >> 1)
#define LEFT_CHILD(i) (i << 1)
#define RIGHT_CHILD(i) ((i << 1) + 1)
#define IS_NODE(i) (i < nodesCount_)
#define IS_LEAF(i) (i >= leafsStart_ && i < leafsEnd_)
#define NODE_TO_LEAF_INDEX(i) (i - leafsStart_)
#define LEAF_TO_NODE_INDEX(i) (i + leafsStart_)

using namespace std;

namespace edcore
{

size_t log2(size_t n)
{
    size_t v = 1;
    for (size_t pow = 1;; pow++)
    {
        v = v << 1;
        if (v >= n)
        {
            return pow;
        }
    }
    return -1;
}

size_t Buffer::memUsage() const
{
    const size_t leafsCount = leafs_.length();

    size_t leafs = 0;
    for (size_t i = 0; i < leafsCount; i++)
    {
        leafs += leafs_[i]->memUsage();
    }
    return (
        sizeof(Buffer) +
        leafs_.memUsage() +
        nodesCount_ * sizeof(BufferNode) +
        leafs);
}

Buffer::Buffer(vector<BufferPiece *> &pieces, size_t minLeafLength, size_t maxLeafLength)
{
    assert(2 * minLeafLength >= maxLeafLength);

    const size_t leafsCount = pieces.size();
    BufferPiece **tmp = new BufferPiece *[leafsCount];
    for (size_t i = 0; i < leafsCount; i++)
    {
        tmp[i] = pieces[i];
    }
    leafs_.init(tmp, leafsCount);

    nodes_ = NULL;
    _rebuildNodes();


    minLeafLength_ = minLeafLength;
    maxLeafLength_ = maxLeafLength;

    // printf("mem usage: %lu B = %lf MB\n", memUsage(), ((double)memUsage()) / 1024 / 1024);
}

void Buffer::_rebuildNodes()
{
    if (nodes_ != NULL)
    {
        delete []nodes_;
    }

    const size_t leafsCount = leafs_.length();

    nodesCount_ = 1 << log2(leafsCount);
    leafsStart_ = nodesCount_;
    leafsEnd_ = leafsStart_ + leafsCount;

    nodes_ = new BufferNode[nodesCount_];
    memset(nodes_, 0, nodesCount_ * sizeof(nodes_[0]));
    for (size_t i = nodesCount_ - 1; i >= 1; i--)
    {
        _updateSingleNode(i);
    }
}

void Buffer::_updateSingleNode(size_t nodeIndex)
{
    size_t left = LEFT_CHILD(nodeIndex);
    size_t right = RIGHT_CHILD(nodeIndex);

    size_t length = 0;
    size_t newLineCount = 0;

    if (IS_NODE(left))
    {
        length += nodes_[left].length;
        newLineCount += nodes_[left].newLineCount;
    }
    else if (IS_LEAF(left))
    {
        length += leafs_[NODE_TO_LEAF_INDEX(left)]->length();
        newLineCount += leafs_[NODE_TO_LEAF_INDEX(left)]->newLineCount();
    }

    if (IS_NODE(right))
    {
        length += nodes_[right].length;
        newLineCount += nodes_[right].newLineCount;
    }
    else if (IS_LEAF(right))
    {
        length += leafs_[NODE_TO_LEAF_INDEX(right)]->length();
        newLineCount += leafs_[NODE_TO_LEAF_INDEX(right)]->newLineCount();
    }

    nodes_[nodeIndex].length = length;
    nodes_[nodeIndex].newLineCount = newLineCount;
}

Buffer::~Buffer()
{
    delete[] nodes_;
    const size_t leafsCount = leafs_.length();
    for (size_t i = 0; i < leafsCount; i++)
    {
        delete leafs_[i];
    }
}

void Buffer::extractString(BufferCursor start, size_t len, uint16_t *dest)
{
    assert(start.offset + len <= nodes_[1].length);

    size_t innerLeafOffset = start.offset - start.leafStartOffset;
    size_t leafIndex = start.leafIndex;
    size_t destOffset = 0;
    while (len > 0)
    {
        BufferPiece *leaf = leafs_[leafIndex];
        const uint16_t *src = leaf->data();
        const size_t cnt = min(len, leaf->length() - innerLeafOffset);
        memcpy(dest + destOffset, src + innerLeafOffset, sizeof(uint16_t) * cnt);
        len -= cnt;
        destOffset += cnt;
        innerLeafOffset = 0;

        if (len == 0)
        {
            break;
        }

        leafIndex++;
    }
}

bool Buffer::findOffset(size_t offset, BufferCursor &result)
{
    if (offset > nodes_[1].length)
    {
        return false;
    }

    size_t it = 1;
    size_t searchOffset = offset;
    size_t leafStartOffset = 0;
    while (!IS_LEAF(it))
    {
        size_t left = LEFT_CHILD(it);
        size_t right = RIGHT_CHILD(it);

        size_t leftLength = 0;
        if (IS_NODE(left))
        {
            leftLength = nodes_[left].length;
        }
        else if (IS_LEAF(left))
        {
            leftLength = leafs_[NODE_TO_LEAF_INDEX(left)]->length();
        }

        if (searchOffset < leftLength || !(IS_NODE(right) || IS_LEAF(right)))
        {
            // go left
            it = left;
        }
        else
        {
            // go right
            searchOffset -= leftLength;
            leafStartOffset += leftLength;
            it = right;
        }
    }
    it = NODE_TO_LEAF_INDEX(it);

    result.offset = offset;
    result.leafIndex = it;
    result.leafStartOffset = leafStartOffset;

    return true;
}

bool Buffer::_findLineStart(size_t &lineIndex, BufferCursor &result)
{
    if (lineIndex > nodes_[1].newLineCount)
    {
        return false;
    }

    size_t it = 1;
    size_t leafStartOffset = 0;
    while (!IS_LEAF(it))
    {
        size_t left = LEFT_CHILD(it);
        size_t right = RIGHT_CHILD(it);

        size_t leftNewLineCount = 0;
        size_t leftLength = 0;
        if (IS_NODE(left))
        {
            leftNewLineCount = nodes_[left].newLineCount;
            leftLength = nodes_[left].length;
        }
        else if (IS_LEAF(left))
        {
            leftNewLineCount = leafs_[NODE_TO_LEAF_INDEX(left)]->newLineCount();
            leftLength = leafs_[NODE_TO_LEAF_INDEX(left)]->length();
        }

        if (lineIndex <= leftNewLineCount)
        {
            // go left
            it = left;
            continue;
        }

        // go right
        lineIndex -= leftNewLineCount;
        leafStartOffset += leftLength;
        it = right;
    }
    it = NODE_TO_LEAF_INDEX(it);

    const LINE_START_T *lineStarts = leafs_[it]->lineStarts();
    const LINE_START_T innerLineStartOffset = (lineIndex == 0 ? 0 : lineStarts[lineIndex - 1]);

    result.offset = leafStartOffset + innerLineStartOffset;
    result.leafIndex = it;
    result.leafStartOffset = leafStartOffset;

    return true;
}

void Buffer::_findLineEnd(size_t leafIndex, size_t leafStartOffset, size_t innerLineIndex, BufferCursor &result)
{
    const size_t leafsCount = leafs_.length();
    while (true)
    {
        BufferPiece *leaf = leafs_[leafIndex];

        if (innerLineIndex < leaf->newLineCount())
        {
            const LINE_START_T *lineStarts = leafs_[leafIndex]->lineStarts();
            LINE_START_T lineEndOffset = lineStarts[innerLineIndex];

            result.offset = leafStartOffset + lineEndOffset;
            result.leafIndex = leafIndex;
            result.leafStartOffset = leafStartOffset;
            return;
        }

        leafIndex++;

        if (leafIndex >= leafsCount)
        {
            result.offset = leafStartOffset + leaf->length();
            result.leafIndex = leafIndex - 1;
            result.leafStartOffset = leafStartOffset;
            return;
        }

        leafStartOffset += leaf->length();
        innerLineIndex = 0;
    }
}

bool Buffer::findLine(size_t lineNumber, BufferCursor &start, BufferCursor &end)
{
    size_t innerLineIndex = lineNumber - 1;
    if (!_findLineStart(innerLineIndex, start))
    {
        return false;
    }

    _findLineEnd(start.leafIndex, start.leafStartOffset, innerLineIndex, end);
    return true;
}

void Buffer::deleteOneOffsetLen(size_t offset, size_t len)
{
    const size_t leafsCount = leafs_.length();
    assert(offset + len <= nodes_[1].length);

    BufferCursor start;
    if (!findOffset(offset, start))
    {
        assert(false);
        return;
    }

    size_t innerLeafOffset = start.offset - start.leafStartOffset;
    size_t leafIndex = start.leafIndex;
    while (len > 0)
    {
        BufferPiece *leaf = leafs_[leafIndex];
        const size_t cnt = min(len, leaf->length() - innerLeafOffset);
        leaf->deleteOneOffsetLen(innerLeafOffset, cnt);
        len -= cnt;
        innerLeafOffset = 0;

        if (len == 0)
        {
            break;
        }

        leafIndex++;
    }

    // Maintain invariant that a leaf does not end in \r or a high surrogate pair
    size_t firstDirtyIndex = start.leafIndex;
    size_t lastDirtyIndex = leafIndex;
    {
        BufferPiece *startLeaf = leafs_[start.leafIndex];
        size_t startLeafLength = startLeaf->length();
        if (startLeafLength > 0)
        {
            uint16_t lastChar = startLeaf->data()[startLeafLength - 1];
            if (lastChar == 13 || (lastChar >= 0xd800 && lastChar <= 0xdbff))
            {
                size_t nextLeafIndex = leafIndex > start.leafIndex ? leafIndex : start.leafIndex + 1;
                BufferPiece *nextLeaf = NULL;
                while (nextLeafIndex < leafsCount)
                {
                    nextLeaf = leafs_[nextLeafIndex];
                    if (nextLeaf->length() > 0)
                    {
                        break;
                    }
                    nextLeafIndex++;
                }

                if (nextLeaf != NULL && nextLeaf->length() > 0)
                {
                    startLeaf->deleteLastChar();
                    nextLeaf->insertFirstChar(lastChar);
                    lastDirtyIndex = nextLeafIndex;
                }
            }
        }
    }

    size_t deleteLeafFrom = leafsCount;
    size_t deleteLeafTo = 0;

    size_t analyzeFrom = max(1UL, firstDirtyIndex);
    size_t analyzeTo = min(lastDirtyIndex + 2, leafsCount);

    BufferPiece *prevLeaf = leafs_[analyzeFrom - 1];
    size_t prevLeafIndex = analyzeFrom - 1;
    bool deletingSomething = false;
    for (size_t i = analyzeFrom; i < analyzeTo; i++)
    {
        size_t prevLeafLength = prevLeaf->length();

        BufferPiece *leaf = leafs_[i];
        size_t leafLength = leaf->length();

        if ((prevLeafLength < minLeafLength_ || leafLength < minLeafLength_) && prevLeafLength + leafLength <= maxLeafLength_)
        {
            firstDirtyIndex = min(firstDirtyIndex, prevLeafIndex);
            prevLeaf->join(leaf);

            // This leaf must be deleted
            deleteLeafFrom = min(deleteLeafFrom, i);
            deleteLeafTo = max(deleteLeafTo, i + 1);
            deletingSomething = true;

            delete leafs_[i];
            leafs_[i] = NULL;

            continue;
        }
        else
        {
            if (deletingSomething)
            {
                // ensure we are deleting only contiguous zones
                break;
            }
        }
        prevLeafIndex = i;
        prevLeaf = leaf;
    }

    if (deleteLeafFrom < deleteLeafTo)
    {
        lastDirtyIndex = leafsCount - 1;
        leafs_.deleteRange(deleteLeafFrom, deleteLeafTo - deleteLeafFrom);
        leafsEnd_ -= (deleteLeafTo - deleteLeafFrom);
    }

    // Check if we should resize our leafs and nodes
    if (deleteLeafFrom < deleteLeafTo)
    {
        size_t newNodesCount = 1 << log2(leafs_.length());
        if (newNodesCount != nodesCount_)
        {
            _rebuildNodes();
            return;
        }
    }

    size_t fromNodeIndex = LEAF_TO_NODE_INDEX(firstDirtyIndex) / 2;
    size_t toNodeIndex = LEAF_TO_NODE_INDEX(lastDirtyIndex) / 2;

    assert(toNodeIndex < nodesCount_);
    _updateNodes(fromNodeIndex, toNodeIndex);
}

void Buffer::insertOneOffsetLen(size_t offset, const uint16_t *data, size_t len)
{
    const size_t leafsCount = leafs_.length();
    assert(offset <= leafsCount);

    BufferCursor start;
    if (!findOffset(offset, start))
    {
        assert(false);
        return;
    }
    
    size_t innerLeafOffset = start.offset - start.leafStartOffset;
    size_t leafIndex = start.leafIndex;
    BufferPiece *leaf = leafs_[leafIndex];

    leaf->insertOneOffsetLen(innerLeafOffset, data, len);

    // TODO: maintain \r and high surrogate invariant

    size_t firstDirtyIndex = leafIndex;
    size_t lastDirtyIndex = leafIndex;

    size_t fromNodeIndex = LEAF_TO_NODE_INDEX(firstDirtyIndex) / 2;
    size_t toNodeIndex = LEAF_TO_NODE_INDEX(lastDirtyIndex) / 2;

    _updateNodes(fromNodeIndex, toNodeIndex);


    // printf("TODO: insertOneOffsetLen @ %lu of length %lu\n", offset, len);
    // printf("cursor: %lu, --(%lu)\n", start.leafIndex, start.leafStartOffset);
}

void Buffer::_updateNodes(size_t fromNodeIndex, size_t toNodeIndex)
{
    while (fromNodeIndex != 0)
    {
        for (size_t nodeIndex = fromNodeIndex; nodeIndex <= toNodeIndex; nodeIndex++)
        {
            _updateSingleNode(nodeIndex);
        }
        fromNodeIndex /= 2;
        toNodeIndex /= 2;
    }
}

void Buffer::assertInvariants()
{
    const size_t leafsCount = leafs_.length();
    assert(leafsCount <= leafs_.capacity());
    assert(leafsStart_ == nodesCount_);
    assert(leafsEnd_ == leafsStart_ + leafsCount);

    for (size_t i = 0; i < leafsCount; i++)
    {
        leafs_[i]->assertInvariants();
    }

    for (size_t i = 1; i < nodesCount_; i++)
    {
        assertNodeInvariants(i);
    }
}

void Buffer::assertNodeInvariants(size_t nodeIndex)
{
    size_t left = LEFT_CHILD(nodeIndex);
    size_t right = RIGHT_CHILD(nodeIndex);

    size_t length = 0;
    size_t newLineCount = 0;

    if (IS_NODE(left))
    {
        length += nodes_[left].length;
        newLineCount += nodes_[left].newLineCount;
    }
    else if (IS_LEAF(left))
    {
        length += leafs_[NODE_TO_LEAF_INDEX(left)]->length();
        newLineCount += leafs_[NODE_TO_LEAF_INDEX(left)]->newLineCount();
    }

    if (IS_NODE(right))
    {
        length += nodes_[right].length;
        newLineCount += nodes_[right].newLineCount;
    }
    else if (IS_LEAF(right))
    {
        length += leafs_[NODE_TO_LEAF_INDEX(right)]->length();
        newLineCount += leafs_[NODE_TO_LEAF_INDEX(right)]->newLineCount();
    }

    assert(nodes_[nodeIndex].length == length);
    assert(nodes_[nodeIndex].newLineCount == newLineCount);
}
}
