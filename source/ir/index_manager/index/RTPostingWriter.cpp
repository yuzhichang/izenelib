#include <ir/index_manager/index/RTPostingWriter.h>
#include <ir/index_manager/index/OutputDescriptor.h>
#include <ir/index_manager/index/InputDescriptor.h>
#include <ir/index_manager/index/SkipListWriter.h>
#include <ir/index_manager/index/SkipListReader.h>
#include <ir/index_manager/store/IndexOutput.h>
#include <ir/index_manager/store/IndexInput.h>

#include <math.h>


using namespace std;

NS_IZENELIB_IR_BEGIN

namespace indexmanager{

RTPostingWriter::RTPostingWriter(MemCache* pCache, int skipInterval, int maxSkipLevel, const string& indexLevel)
        :pMemCache_(pCache)
        ,skipInterval_(skipInterval)
        ,maxSkipLevel_(maxSkipLevel)
        ,nDF_(0)
        ,nLastDocID_(BAD_DOCID)
        ,nLastLoc_(BAD_DOCID)
        ,nCurTermFreq_(0)
        ,nCTF_(0)
        ,pSkipListWriter_(0)
	    ,dirty_(false)
        ,indexLevel_(indexLevel)
{
    pDocFreqList_ = new VariantDataPool(pCache);
    if(indexLevel == "wordlevel")
        pLocList_  = new VariantDataPool(pCache);
    else
        pLocList_ = NULL;
    if(skipInterval_> 0 && maxSkipLevel_ > 0)
        pSkipListWriter_ = new SkipListWriter(skipInterval_,maxSkipLevel_,pMemCache_);
}

RTPostingWriter::~RTPostingWriter()
{
    if (pDocFreqList_)
    {
        delete pDocFreqList_;
        pDocFreqList_ = NULL;
    }
    if (pLocList_)
    {
        delete pLocList_;
        pLocList_ = NULL;
    }
    pMemCache_ = NULL;

    if(pSkipListWriter_)
    {
        delete pSkipListWriter_;
        pSkipListWriter_ = 0;
    }
}

bool RTPostingWriter::isEmpty()
{
    return (pDocFreqList_->pTailChunk_==NULL);
}

void RTPostingWriter::write(OutputDescriptor* pOutputDescriptor, TermInfo& termInfo)
{
    ///flush last document
    flushLastDoc(true);

    termInfo.docFreq_ = nDF_;
    termInfo.ctf_ = nCTF_;
    termInfo.lastDocID_ = nLastDocID_;

    if(pSkipListWriter_ && nDF_ > 0 && nDF_ % skipInterval_ == 0)
        pSkipListWriter_->addSkipPoint(nLastDocID_,pDocFreqList_->getLength(),pLocList_->getLength());

    IndexOutput* pDOutput = pOutputDescriptor->getDPostingOutput();

    if( pSkipListWriter_ && pSkipListWriter_->getNumLevels() > 0) ///nDF_ > SkipInterval
    {
        termInfo.skipLevel_ = pSkipListWriter_->getNumLevels();
        termInfo.skipPointer_ = pDOutput->getFilePointer();
        pSkipListWriter_->write(pDOutput);	///write skip list data
    }
    else
    {
        termInfo.skipPointer_ = -1;
        termInfo.skipLevel_ = 0;
    }

    ///save the offset of posting descriptor
    termInfo.docPointer_ = pDOutput->getFilePointer();

    ///write doc posting data
    pDocFreqList_->write(pDOutput);
	
    termInfo.docPostingLen_ = pDOutput->getFilePointer() - termInfo.docPointer_;

    if(indexLevel_ == "wordlevel")
    {
        IndexOutput* pPOutput = pOutputDescriptor->getPPostingOutput();

        termInfo.positionPointer_ = pPOutput->getFilePointer();

        ///write position posting data
        if (pLocList_)
            pLocList_->write(pPOutput);

        termInfo.positionPostingLen_ = pPOutput->getFilePointer() - termInfo.positionPointer_;
    }
}

void RTPostingWriter::reset()
{
    pDocFreqList_->reset();
    if(indexLevel_ == "wordlevel" && pLocList_)
        pLocList_->reset();

    nCTF_ = 0;
    nLastDocID_ = BAD_DOCID;
    nLastLoc_ = 0;
    nDF_ = 0;
    nCurTermFreq_ = 0;

    if(pSkipListWriter_)
        pSkipListWriter_ ->reset();
}

void RTPostingWriter::add(docid_t docid, loc_t location)
{
    if (docid == nLastDocID_)
    {
        ///see it before,only position is needed
        if(indexLevel_ == "wordlevel" && pLocList_)
            pLocList_->addVData32(location - nLastLoc_);
        nCurTermFreq_++;
        nLastLoc_ = location;
    }
    else///first see it
    {
        if (nCurTermFreq_ > 0)///write previous document's term freq
        {
            pDocFreqList_->addVData32(nCurTermFreq_);
        }
        else if (nLastDocID_ == BAD_DOCID)///first see it
        {
            nLastDocID_ = 0;
        }

        if(pSkipListWriter_ && nDF_ > 0 && nDF_ % skipInterval_ == 0)
        {
            if(pLocList_)
                pSkipListWriter_->addSkipPoint(nLastDocID_,pDocFreqList_->getLength(),pLocList_->getLength());
            else
                pSkipListWriter_->addSkipPoint(nLastDocID_,pDocFreqList_->getLength(),0);
        }


        pDocFreqList_->addVData32(docid - nLastDocID_);

        if(indexLevel_ == "wordlevel" && pLocList_)
            pLocList_->addVData32(location);

        nCTF_ += nCurTermFreq_;
        nCurTermFreq_ = 1;

        nLastDocID_ = docid;
        nLastLoc_ = location;

        nDF_++;
    }
}

int32_t RTPostingWriter::getSkipLevel()
{
    if( pSkipListWriter_ && pSkipListWriter_->getNumLevels() > 0)
        return pSkipListWriter_->getNumLevels();
    else
        return 0;
}

void RTPostingWriter::flushLastDoc(bool bTruncTail)
{
    if(!pMemCache_)
        return;

    if (nCurTermFreq_ > 0)
    {
        pDocFreqList_->addVData32(nCurTermFreq_);
        if (bTruncTail)
        {
            pDocFreqList_->truncTailChunk();///update real size
            if( indexLevel_ == "wordlevel" && pLocList_ )
                pLocList_->truncTailChunk();///update real size
        }
        nCTF_ += nCurTermFreq_;
        nCurTermFreq_ = 0;
    }
    else if (bTruncTail)
    {
        pDocFreqList_->truncTailChunk();///update real size
        if(indexLevel_ == "wordlevel" && pLocList_)
            pLocList_->truncTailChunk();///update real size
    }
}

PostingReader* RTPostingWriter::createPostingReader()
{
    return new MemPostingReader(this);
}

}

NS_IZENELIB_IR_END

