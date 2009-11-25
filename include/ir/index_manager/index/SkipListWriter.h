/**
* @file        SkipListWriter.h
* @author     Yingfeng Zhang
* @version     SF1 v5.0
* @brief Writing skiplist data
*/

#ifndef SKIPLIST_WRITER_H
#define SKIPLIST_WRITER_H

#include <ir/index_manager/index/VariantDataPool.h>
#include <ir/index_manager/utility/MemCache.h>

NS_IZENELIB_IR_BEGIN

namespace indexmanager{

/**
 * This class writes skip lists with multiple levels.
 * 
 * Example for skipInterval = 3:
 *                                                     c            (skip level 2)
 *                 c                 c                 c            (skip level 1) 
 *     x     x     x     x     x     x     x     x     x     x      (skip level 0)
 * d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d d  (posting list)
 *     3     6     9     12    15    18    21    24    27    30     (df)
 * 
 * d - document
 * x - skip data
 * c - skip data with child pointer
 * 
 * Skip level i contains every skipInterval-th entry from skip level i-1.
 * Therefore the number of entries on level i is: floor(df / ((skipInterval ^ (i + 1))).
 * 
 * Each skip entry on a level i>0 contains a pointer to the corresponding skip entry in list i-1.
 * This guarantess a logarithmic amount of skips to find the target document.
 * 
 * While this class takes care of writing the different skip levels,
 * subclasses must define the actual format of the skip data.
 * 
 */
class SkipListWriter
{
public:
    SkipListWriter(int skipInterval, int maxLevel, MemCache* pMemCache);

    virtual ~SkipListWriter();

public:
    int getMaxSkipLevel() { return maxSkipLevel_;}

    int getNumLevels()
    {
        int nNumLevls = 0;
        for(int i = 0; i < maxSkipLevel_; i++)
        {
            if(ppSkipLevels_[i] && ppSkipLevels_[i]->getRealSize() > 0)
                nNumLevls++;
            else
                break;
        }
        return nNumLevls;
    }

    void addSkipPoint();

    void write(IndexOutput* pOutput);

    void reset();
private:
    VariantDataPool** ppSkipLevels_;

    int skipInterval_;

    int maxSkipLevel_;

    int numPointsInLowestLevel_;

    ///each level should record the last doc id, last fileoffset of both dfp and pos postings
    docid_t* pLastDoc_; 
    fileoffset_t* pLastOffset_;
    fileoffset_t* pLastPOffset_;

    docid_t curDoc_;
    fileoffset_t curOffset_;
    fileoffset_t curPOffset_;
};

}
NS_IZENELIB_IR_END

#endif
