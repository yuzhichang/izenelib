#include <ir/Zambezi/SegmentPool.hpp>
#include <ir/Zambezi/pfordelta/opt_p4.hpp>
#include <ir/Zambezi/bloom/BloomFilter.hpp>
#include <ir/Zambezi/Utils.hpp>

#include <algorithm>
#include <cstring>

NS_IZENELIB_IR_BEGIN

namespace Zambezi
{

SegmentPool::SegmentPool(
        uint32_t numberOfPools, bool reverse, bool bloomEnabled,
        bool nbHash, bool bitsPerElement)
    : numberOfPools_(numberOfPools)
    , segment_(0)
    , offset_(0)
    , reverse_(reverse)
    , bloomEnabled_(bloomEnabled)
    , nbHash_(nbHash)
    , bitsPerElement_(bitsPerElement)
    , pool_(numberOfPools)
{
}

SegmentPool::~SegmentPool()
{
}

void SegmentPool::save(std::ostream& ostr) const
{
    ostr.write((const char*)&numberOfPools_, sizeof(numberOfPools_));
    ostr.write((const char*)&segment_, sizeof(segment_));
    ostr.write((const char*)&offset_, sizeof(offset_));
    ostr.write((const char*)&reverse_, sizeof(reverse_));
    ostr.write((const char*)&bloomEnabled_, sizeof(bloomEnabled_));
    ostr.write((const char*)&nbHash_, sizeof(nbHash_));
    ostr.write((const char*)&bitsPerElement_, sizeof(bitsPerElement_));

    for (size_t i = 0; i < segment_; ++i)
    {
        ostr.write((const char*)&pool_[i][0], sizeof(pool_[i][0]) * MAX_POOL_SIZE);
    }
    ostr.write((const char*)&pool_[segment_][0], sizeof(pool_[segment_][0]) * offset_);
}

void SegmentPool::load(std::istream& istr)
{
    istr.read((char*)&numberOfPools_, sizeof(numberOfPools_));
    istr.read((char*)&segment_, sizeof(segment_));
    istr.read((char*)&offset_, sizeof(offset_));
    istr.read((char*)&reverse_, sizeof(reverse_));
    istr.read((char*)&bloomEnabled_, sizeof(bloomEnabled_));
    istr.read((char*)&nbHash_, sizeof(nbHash_));
    istr.read((char*)&bitsPerElement_, sizeof(bitsPerElement_));

    pool_.resize(segment_ + 1);
    for (size_t i = 0; i < segment_; ++i)
    {
        pool_[i].resize(MAX_POOL_SIZE);
        istr.read((char *)&pool_[i][0], sizeof(pool_[i][0]) * MAX_POOL_SIZE);
    }
    pool_[segment_].resize(MAX_POOL_SIZE);
    istr.read((char*)&pool_[segment_][0], sizeof(pool_[segment_][0]) * offset_);
}

bool SegmentPool::isTermFrequencyPresent() const
{
    uint32_t reqspace = pool_[0][0];
    uint32_t csize = pool_[0][4];
    if (csize + 5 == reqspace)
    {
        return false;
    }
    return true;
}

bool SegmentPool::isPositional() const
{
    uint32_t reqspace = pool_[0][0];
    uint32_t csize = pool_[0][4];
    if (csize + 5 == reqspace)
    {
        return false;
    }
    uint32_t tfcsize = pool_[0][csize + 5];
    if (csize + tfcsize + 6 == reqspace)
    {
        return false;
    }
    return true;
}

size_t SegmentPool::compressAndAddNonPositional(
        uint32_t* docid_list,
        uint32_t len, size_t tailPointer)
{
    uint32_t lastSegment = UNDEFINED_SEGMENT;
    uint32_t lastOffset = 0;
    if (tailPointer != UNDEFINED_POINTER)
     {
        lastSegment = DECODE_SEGMENT(tailPointer);
        lastOffset = DECODE_OFFSET(tailPointer);
    }

    uint32_t* filter = 0;
    uint32_t filterSize = 0;
    if (bloomEnabled_)
    {
        filterSize = BloomFilter::computeLength(len, bitsPerElement_);
        filter = (uint32_t*) calloc(filterSize, sizeof(uint32_t));
        for (uint32_t i = 0; i < len; ++i)
        {
            BloomFilter::insert(filter, filterSize, nbHash_, docid_list[i]);
        }
    }

    uint32_t maxDocId = reverse_ ? docid_list[0] : docid_list[len - 1];

    if (reverse_)
    {
        std::reverse(docid_list, docid_list + len - 1);
    }

    uint32_t* block = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
    uint32_t csize = OPT4(docid_list, len, block, 1);

    uint32_t reqspace = csize + filterSize + 8;
    if (reqspace > MAX_POOL_SIZE - offset_)
    {
        ++segment_;
        offset_ = 0;
    }

    if (pool_[segment_].empty())
    {
        pool_[segment_].resize(MAX_POOL_SIZE);
    }

    pool_[segment_][offset_] = reqspace;
    pool_[segment_][offset_ + 1] = UNDEFINED_SEGMENT;
    pool_[segment_][offset_ + 2] = 0;
    pool_[segment_][offset_ + 3] = maxDocId;
    pool_[segment_][offset_ + 4] = csize + 7;
    pool_[segment_][offset_ + 5] = len;
    pool_[segment_][offset_ + 6] = csize;

    memcpy(&pool_[segment_][offset_ + 7],
            block, csize * sizeof(uint32_t));

    if (filter)
    {
        pool_[segment_][offset_ + csize + 7] = filterSize;
        memcpy(&pool_[segment_][offset_ + csize + 8],
                filter, filterSize * sizeof(uint32_t));
    }

    if (lastSegment != UNDEFINED_SEGMENT)
    {
        if (reverse_)
        {
            pool_[segment_][offset_ + 1] = lastSegment;
            pool_[segment_][offset_ + 2] = lastOffset;
        }
        else
        {
            pool_[lastSegment][lastOffset + 1] = segment_;
            pool_[lastSegment][lastOffset + 2] = offset_;
        }
    }

    size_t newPointer = ENCODE_POINTER(segment_, offset_);
    offset_ += reqspace;

    free(block);
    if (filter) free(filter);

    return newPointer;
}

size_t SegmentPool::compressAndAddTfOnly(
        uint32_t* docid_list, uint32_t* tf_list,
        uint32_t len, size_t tailPointer)
{
    uint32_t lastSegment = UNDEFINED_SEGMENT;
    uint32_t lastOffset = 0;
    if (tailPointer != UNDEFINED_POINTER)
    {
        lastSegment = DECODE_SEGMENT(tailPointer);
        lastOffset = DECODE_OFFSET(tailPointer);
    }

    uint32_t* filter = 0;
    uint32_t filterSize = 0;
    if (bloomEnabled_)
    {
        filterSize = BloomFilter::computeLength(len, bitsPerElement_);
        filter = (uint32_t*) calloc(filterSize, sizeof(uint32_t));
        for (uint32_t i = 0; i < len; ++i)
        {
            BloomFilter::insert(filter, filterSize, nbHash_, docid_list[i]);
        }
    }

    uint32_t maxDocId = reverse_ ? docid_list[0] : docid_list[len - 1];

    if (reverse_)
    {
        std::reverse(docid_list, docid_list + len - 1);
        std::reverse(tf_list, tf_list + len - 1);
    }

    uint32_t* block = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
    uint32_t* tfblock = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
    uint32_t csize = OPT4(docid_list, len, block, 1);
    uint32_t tfcsize = OPT4(tf_list, len, tfblock, 0);

    uint32_t reqspace = csize + tfcsize + filterSize + 9;
    if (reqspace > MAX_POOL_SIZE - offset_)
    {
        ++segment_;
        offset_ = 0;
    }

    if (pool_[segment_].empty())
    {
        pool_[segment_].resize(MAX_POOL_SIZE);
    }

    pool_[segment_][offset_] = reqspace;
    pool_[segment_][offset_ + 1] = UNDEFINED_SEGMENT;
    pool_[segment_][offset_ + 2] = 0;
    pool_[segment_][offset_ + 3] = maxDocId;
    pool_[segment_][offset_ + 4] = csize + tfcsize + 8;
    pool_[segment_][offset_ + 5] = len;
    pool_[segment_][offset_ + 6] = csize;

    memcpy(&pool_[segment_][offset_ + 7],
            block, csize * sizeof(uint32_t));

    pool_[segment_][offset_ + 7 + csize] = tfcsize;
    memcpy(&pool_[segment_][offset_ + 8 + csize],
            tfblock, tfcsize * sizeof(uint32_t));

    if (filter)
    {
        pool_[segment_][offset_ + csize + tfcsize + 8] = filterSize;
        memcpy(&pool_[segment_][offset_ + 9 + csize + tfcsize],
                filter, filterSize * sizeof(uint32_t));
    }

    if (lastSegment != UNDEFINED_SEGMENT)
    {
        if (reverse_)
        {
            pool_[segment_][offset_ + 1] = lastSegment;
            pool_[segment_][offset_ + 2] = lastOffset;
        }
        else
        {
            pool_[lastSegment][lastOffset + 1] = segment_;
            pool_[lastSegment][lastOffset + 2] = offset_;
        }
    }

    size_t newPointer = ENCODE_POINTER(segment_, offset_);
    offset_ += reqspace;

    free(block);
    free(tfblock);
    if (filter) free(filter);

    return newPointer;
}

size_t SegmentPool::compressAndAddPositional(
        uint32_t* docid_list, uint32_t* tf_list, uint32_t* position_list,
        uint32_t len, uint32_t plen, size_t tailPointer)
{
    uint32_t lastSegment = UNDEFINED_SEGMENT;
    uint32_t lastOffset = 0;
    if (tailPointer != UNDEFINED_POINTER)
    {
        lastSegment = DECODE_SEGMENT(tailPointer);
        lastOffset = DECODE_OFFSET(tailPointer);
    }

    uint32_t* filter = 0;
    uint32_t filterSize = 0;
    if (bloomEnabled_)
    {
        filterSize = BloomFilter::computeLength(len, bitsPerElement_);
        filter = (uint32_t*) calloc(filterSize, sizeof(uint32_t));
        for (uint32_t i = 0; i < len; ++i)
        {
            BloomFilter::insert(filter, filterSize, nbHash_, docid_list[i]);
        }
    }

    uint32_t maxDocId = reverse_ ? docid_list[0] : docid_list[len - 1];

    if (reverse_)
    {
        if (position_list)
        {
            uint32_t* rpositions = (uint32_t*) calloc(plen, sizeof(uint32_t*));
            uint32_t curPos = plen;
            for (uint32_t i = 0; i < len; ++i)
            {
                curPos -= tf_list[i];
                memcpy(&rpositions[curPos], position_list, tf_list[i] * sizeof(uint32_t));
                position_list += tf_list[i];
            }
            position_list = rpositions;
        }

        std::reverse(docid_list, docid_list + len - 1);
        std::reverse(tf_list, tf_list + len - 1);
    }

    uint32_t pblocksize = 3 * (plen / BLOCK_SIZE + 1) * BLOCK_SIZE;
    uint32_t* block = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
    uint32_t* tfblock = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
    uint32_t* pblock = (uint32_t*) calloc(pblocksize, sizeof(uint32_t));
    uint32_t csize = OPT4(docid_list, len, block, true);
    uint32_t tfcsize = OPT4(tf_list, len, tfblock, false);

    uint32_t pcsize = 0;
    uint32_t nb = plen / BLOCK_SIZE;
    uint32_t res = plen % BLOCK_SIZE;

    for (uint32_t i = 0; i < nb; ++i)
    {
        uint32_t tempPcsize = OPT4(&position_list[i * BLOCK_SIZE], BLOCK_SIZE, &pblock[pcsize + 1], false);
        pblock[pcsize] = tempPcsize;
        pcsize += tempPcsize + 1;
    }

    if (res > 0)
    {
        uint32_t tempPcsize = OPT4(&position_list[nb * BLOCK_SIZE], res, &pblock[pcsize + 1], false);
        pblock[pcsize] = tempPcsize;
        pcsize += tempPcsize + 1;
    }

    uint32_t reqspace = csize + tfcsize + pcsize + filterSize + 11;
    if (reqspace > MAX_POOL_SIZE - offset_)
    {
        ++segment_;
        offset_ = 0;
    }

    if (pool_[segment_].empty())
    {
        pool_[segment_].resize(MAX_POOL_SIZE);
    }

    pool_[segment_][offset_] = reqspace;
    pool_[segment_][offset_ + 1] = UNDEFINED_SEGMENT;
    pool_[segment_][offset_ + 2] = 0;
    pool_[segment_][offset_ + 3] = maxDocId;
    pool_[segment_][offset_ + 4] = csize + tfcsize + pcsize + 10;
    pool_[segment_][offset_ + 5] = len;
    pool_[segment_][offset_ + 6] = csize;

    memcpy(&pool_[segment_][offset_ + 7],
            block, csize * sizeof(uint32_t));

    pool_[segment_][offset_ + 7 + csize] = tfcsize;
    memcpy(&pool_[segment_][offset_ + 8 + csize], tfblock, tfcsize * sizeof(uint32_t));

    pool_[segment_][offset_ + 8 + csize + tfcsize] = plen;
    pool_[segment_][offset_ + 9 + csize + tfcsize] = nb + (res ? 1 : 0);
    memcpy(&pool_[segment_][offset_ + 10 + csize + tfcsize],
            pblock, pcsize * sizeof(uint32_t));

    if (filter)
    {
        pool_[segment_][offset_ + csize + tfcsize + pcsize + 10] = filterSize;
        memcpy(&pool_[segment_][offset_ + 11 + csize + tfcsize + pcsize],
                filter, filterSize * sizeof(uint32_t));
    }

    if (lastSegment != UNDEFINED_SEGMENT)
    {
        if (reverse_)
        {
            pool_[segment_][offset_ + 1] = lastSegment;
            pool_[segment_][offset_ + 2] = lastOffset;
        }
        else
        {
            pool_[lastSegment][lastOffset + 1] = segment_;
            pool_[lastSegment][lastOffset + 2] = offset_;
        }
    }

    size_t newPointer = ENCODE_POINTER(segment_, offset_);
    offset_ += reqspace;

    if (reverse_)
    {
        free(position_list);
    }
    free(block);
    free(tfblock);
    free(pblock);
    free(filter);

    return newPointer;
}

size_t SegmentPool::nextPointer(size_t pointer) const
{
    if (pointer == UNDEFINED_POINTER)
    {
        return UNDEFINED_POINTER;
    }

    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    if (pool_[pSegment][pOffset + 1] == UNDEFINED_SEGMENT)
    {
        return UNDEFINED_POINTER;
    }

    return ENCODE_POINTER(pool_[pSegment][pOffset + 1], pool_[pSegment][pOffset + 2]);
}

uint32_t SegmentPool::decompressDocidBlock(uint32_t* outBlock, size_t pointer) const
{
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    uint32_t aux[BLOCK_SIZE*4];
    const uint32_t* block = &pool_[pSegment][pOffset + 7];
    detailed_p4_decode(outBlock, block, aux, true, reverse_);

    return pool_[pSegment][pOffset + 5];
}

uint32_t SegmentPool::decompressTfBlock(uint32_t* outBlock, size_t pointer) const
{
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    uint32_t aux[BLOCK_SIZE*4];
    uint32_t csize = pool_[pSegment][pOffset + 6];
    const uint32_t* block = &pool_[pSegment][pOffset + csize + 8];
    detailed_p4_decode(outBlock, block, aux, false, reverse_);

    return pool_[pSegment][pOffset + 5];
}

uint32_t SegmentPool::numberOfPositionBlocks(size_t pointer) const
{
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    uint32_t csize = pool_[pSegment][pOffset + 6];
    uint32_t tfsize = pool_[pSegment][pOffset + 7 + csize];
    return pool_[pSegment][pOffset + csize + tfsize + 9];
}

uint32_t SegmentPool::decompressPositionBlock(uint32_t* outBlock, size_t pointer) const
{
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    uint32_t aux[BLOCK_SIZE * 4];
    uint32_t csize = pool_[pSegment][pOffset + 6];
    uint32_t tfsize = pool_[pSegment][pOffset + 7 + csize];
    uint32_t nb = pool_[pSegment][pOffset + csize + tfsize + 9];

    uint32_t index = pOffset + csize + tfsize + 10;
    for (uint32_t i = 0; i < nb; ++i)
    {
        uint32_t sb = pool_[pSegment][index];
        detailed_p4_decode(&outBlock[i * BLOCK_SIZE], &pool_[pSegment][index + 1], aux, false, reverse_);
        memset(aux, 0, BLOCK_SIZE * 4 * sizeof(uint32_t));
        index += sb + 1;
    }
    return pool_[pSegment][pOffset + csize + tfsize + 8];
}

void SegmentPool::decompressPositions(uint32_t* tf_list, uint32_t index, size_t pointer, uint32_t* out) const
{
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    uint32_t aux[BLOCK_SIZE*4];
    uint32_t csize = pool_[pSegment][pOffset + 6];
    uint32_t tfsize = pool_[pSegment][pOffset + 7 + csize];
    uint32_t nb = 0;
    for (uint32_t i = 0; i < index; ++i)
    {
        nb += tf_list[i];
    }
    uint32_t lnb = nb + tf_list[index] - 1;
    uint32_t r = nb % BLOCK_SIZE;
    nb = nb / BLOCK_SIZE;
    lnb = lnb / BLOCK_SIZE;

    uint32_t pos = pOffset + csize + tfsize + 10;
    for (uint32_t i = 0; i < nb; ++i)
    {
        pos += pool_[pSegment][pos] + 1;
    }
    uint32_t cindex = 0, left = tf_list[index], tocopy = tf_list[index], rindex = r;
    for (uint32_t i = nb; i <= lnb; ++i)
    {
        if (rindex + tocopy > BLOCK_SIZE)
        {
            tocopy = BLOCK_SIZE - rindex;
        }
        const uint32_t* block = &pool_[pSegment][pos + 1];
        uint32_t* temp = (uint32_t*) calloc(BLOCK_SIZE * 2, sizeof(uint32_t));
        detailed_p4_decode(temp, block, aux, false, reverse_);
        memcpy(&out[cindex], &temp[rindex], tocopy * sizeof(uint32_t));
        pos += pool_[pSegment][pos] + 1;
        free(temp);

        cindex += tocopy;
        left -= tocopy;
        tocopy = left;
        rindex = 0;
    }
    for (uint32_t i = 1; i < tf_list[index]; ++i)
    {
        out[i] += out[i - 1];
    }
}

bool SegmentPool::containsDocid(uint32_t docid, size_t& pointer) const
{
    if (pointer == UNDEFINED_POINTER)
    {
        return false;
    }
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    while (LESS_THAN(pool_[pSegment][pOffset + 3], docid, reverse_))
    {
        pSegment = pool_[pSegment][pOffset + 1];
        pOffset = pool_[pSegment][pOffset + 2];
        if (pSegment == UNDEFINED_SEGMENT)
        {
            pointer = UNDEFINED_POINTER;
            return false;
        }
    }

    if (pool_[pSegment][pOffset + 3] == docid)
    {
        return true;
    }

    uint32_t bloomOffset = pool_[pSegment][pOffset + 4];
    pointer = ENCODE_POINTER(pSegment, pOffset);
    return BloomFilter::contains(
            &pool_[pSegment][pOffset + bloomOffset + 1],
            pool_[pSegment][pOffset + bloomOffset],
            nbHash_, docid);
}

size_t SegmentPool::readPostingsForTerm(std::istream& istr, size_t pointer)
{
    uint32_t sSegment = UNDEFINED_SEGMENT, ppSegment = UNDEFINED_SEGMENT;
    uint32_t sOffset = 0, ppOffset = 0;
    uint32_t pSegment = DECODE_SEGMENT(pointer);
    uint32_t pOffset = DECODE_OFFSET(pointer);

    while (pSegment != UNDEFINED_SEGMENT)
    {
        size_t pos = (pSegment * MAX_POOL_SIZE + pOffset) * 4 + 24;

        istr.seekg(pos, istr.beg);
        uint32_t reqspace = 0;
        istr.read((char*)&reqspace, sizeof(uint32_t));

        if (reqspace > MAX_POOL_SIZE - offset_)
        {
            ++segment_;
            offset_ = 0;
        }

        pool_[segment_][offset_] = reqspace;
        istr.read((char*)&pool_[segment_][offset_ + 1], sizeof(uint32_t) * (reqspace - 1));

        pSegment = pool_[segment_][offset_ + 1];
        pOffset = pool_[segment_][offset_ + 2];

        if (ppSegment != UNDEFINED_SEGMENT)
        {
            pool_[ppSegment][ppOffset + 1] = segment_;
            pool_[ppSegment][ppOffset + 2] = offset_;
        }

        if (sSegment == UNDEFINED_SEGMENT)
        {
            sSegment = segment_;
            sOffset = offset_;
        }

        ppSegment = segment_;
        ppOffset = offset_;

        offset_ += reqspace;
    }

    return ENCODE_POINTER(sSegment, sOffset);
}

}

NS_IZENELIB_IR_END