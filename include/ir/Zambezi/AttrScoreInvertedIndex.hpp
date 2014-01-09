#ifndef IZENELIB_IR_ZAMBEZI_ATTR_SCORE_INVERTED_INDEX_HPP
#define IZENELIB_IR_ZAMBEZI_ATTR_SCORE_INVERTED_INDEX_HPP

#include "IndexBase.hpp"
#include "SegmentPool.hpp"
#include "Dictionary.hpp"
#include "Pointers.hpp"
#include "buffer/AttrScoreBufferMaps.hpp"
#include "Utils.hpp"
#include "Consts.hpp"
#include <util/compression/int/fastpfor/fastpfor.h>

#include <iostream>
#include <vector>
#include <glog/logging.h>


NS_IZENELIB_IR_BEGIN

namespace Zambezi
{

class AttrScoreInvertedIndex : public IndexBase
{
public:
    AttrScoreInvertedIndex(
            uint32_t maxPoolSize = MAX_POOL_SIZE,
            uint32_t numberOfPools = NUMBER_OF_POOLS,
            uint32_t vocabSize = DEFAULT_VOCAB_SIZE,
            bool reverse = true);

    ~AttrScoreInvertedIndex();

    void save(std::ostream& ostr) const;
    void load(std::istream& istr);


    /// @brief: interface to build AttrScore zambezi index;
    /// @docid: must be used;
    /// @term_list:
    /// for example: if the title is:"aa bb cc dd aa cc";
    /// then, the @term_list is {aa, bb, cc, dd}, each @term in @term_list should be unique;
    /// @score_list: the score of its term;
    void insertDoc(
            uint32_t docid,
            const std::vector<std::string>& term_list,
            const std::vector<uint32_t>& score_list);

    void flush();

    uint32_t totalDocNum() const;

    void retrieve(
            Algorithm algorithm,
            const std::vector<std::pair<std::string, int> >& term_list,
            const FilterBase* filter,
            uint32_t hits,
            std::vector<uint32_t>& docid_list,
            std::vector<float>& score_list) const;

private:
    void processTermBuffer_(
            AttrScoreBufferMaps::PostingType& posting,
            size_t& tailPointer,
            size_t& headPointer);

    size_t compressAndAppendBlock_(
            uint32_t* docBlock,
            uint32_t* scoreBlock,
            uint32_t len,
            size_t lastPointer,
            size_t nextPointer);

    uint32_t decompressDocidBlock_(
            FastPFor& codec,
            uint32_t* outBlock, size_t pointer) const;

    uint32_t decompressScoreBlock_(
            FastPFor& codec,
            uint32_t* outBlock, size_t pointer) const;

    void intersectSvS_(
            const std::vector<uint32_t>& qTerms,
            const std::vector<int>& qScores,
            const FilterBase* filter,
            uint32_t minDf,
            uint32_t hits,
            std::vector<uint32_t>& docid_list,
            std::vector<float>& score_list) const;

    bool unionIterate_(
            FastPFor& codec,
            bool in_buffer,
            const boost::shared_ptr<AttrScoreBufferMaps::PostingType>& buffer,
            uint32_t segment[],
            uint32_t count,
            uint32_t index,
            size_t pointer,
            uint32_t pivot) const;

    template <class BlockType>
    uint32_t gallopSearch_(
            FastPFor& codec,
            const BlockType& block,
            uint32_t count,
            uint32_t index,
            uint32_t pivot) const
    {
        if (GREATER_THAN_EQUAL((uint32_t)block[index], pivot, pool_.reverse_))
            return index;

        if ((uint32_t)block[count - 1] == pivot)
            return count - 1;

        int beginIndex = index;
        int hop = 1;
        int tempIndex = beginIndex + 1;
        while ((uint32_t)tempIndex < count && LESS_THAN_EQUAL((uint32_t)block[tempIndex], pivot, pool_.reverse_))
        {
            beginIndex = tempIndex;
            tempIndex += hop;
            hop *= 2;
        }

        if ((uint32_t)block[beginIndex] == pivot)
            return beginIndex;

        int endIndex = count - 1;
        hop = 1;
        tempIndex = endIndex - 1;
        while (tempIndex >= 0 && GREATER_THAN((uint32_t)block[tempIndex], pivot, pool_.reverse_))
        {
            endIndex = tempIndex;
            tempIndex -= hop;
            hop *= 2;
        }

        if ((uint32_t)block[endIndex] == pivot)
            return endIndex;

        // Binary search between begin and end indexes
        while (beginIndex < endIndex)
        {
            uint32_t mid = beginIndex + (endIndex - beginIndex) / 2;

            if (GREATER_THAN((uint32_t)block[mid], pivot, pool_.reverse_))
            {
                endIndex = mid;
            }
            else if (LESS_THAN((uint32_t)block[mid], pivot, pool_.reverse_))
            {
                beginIndex = mid + 1;
            }
            else
            {
                return mid;
            }
        }

        return endIndex;
    }

    void intersectPostingsLists_(
            FastPFor& codec,
            const FilterBase* filter,
            uint32_t term0,
            uint32_t term1,
            int weight0,
            int weight1,
            std::vector<uint32_t>& docid_list,
            std::vector<float>& score_list) const;

    void intersectSetPostingsList_(
            FastPFor& codec,
            uint32_t term,
            int weight,
            std::vector<uint32_t>& docid_list,
            std::vector<float>& score_list) const;

private:
    AttrScoreBufferMaps buffer_;
    SegmentPool pool_;
    Dictionary<std::string> dictionary_;
    Pointers pointers_;

    FastPFor codec_;

    static const size_t BUFFER_SIZE = 1024;
    uint32_t segment_[BUFFER_SIZE];
};

}

NS_IZENELIB_IR_END

#endif
