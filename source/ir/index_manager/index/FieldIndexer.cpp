#include <ir/index_manager/index/FieldIndexer.h>
#include <ir/index_manager/index/TermReader.h>
#include <ir/index_manager/index/TermPositions.h>

using namespace std;

using namespace izenelib::ir::indexmanager;

FieldIndexer::FieldIndexer(MemCache* pCache):pMemCache_(pCache),vocFilePointer_(0)
{
}

FieldIndexer::~FieldIndexer()
{
    for(InMemoryPostingMap::iterator iter = postingMap_.begin(); iter !=postingMap_.end(); ++iter)
    {
        delete iter->second;
    }

    pMemCache_ = NULL;
}

void FieldIndexer::addField(docid_t docid, boost::shared_ptr<LAInput> laInput)
{
    InMemoryPosting* curPosting;

    for(LAInput::iterator iter = laInput->begin(); iter != laInput->end(); ++iter)
    {
        curPosting = (InMemoryPosting*)postingMap_[iter->termId_];
        if (curPosting == NULL)
        {
            curPosting = new InMemoryPosting(pMemCache_);
            postingMap_[iter->termId_] = curPosting;
        }
        curPosting->addLocation(docid, iter->wordOffset_, iter->byteOffset_);
        curPosting->updateDF(docid);
    }
}

void FieldIndexer::removeField(docid_t docid, boost::shared_ptr<LAInput> laInput)
{
    docid_t decompressed_docid;

    InMemoryPosting* newPosting;

    InMemoryTermReader* pTermReader = new InMemoryTermReader(getField(),this);

    for(LAInput::iterator iter = laInput->begin(); iter != laInput->end(); ++iter)
    {
        termid_t termId = (termid_t)iter->termId_;

        Term term(getField(), termId);
        if (pTermReader->seek(&term))
        {
            newPosting = new InMemoryPosting(pMemCache_);
            TermPositions* pTermPositions = pTermReader->termPositions();
            while (pTermPositions->next())
            {
                decompressed_docid = pTermPositions->doc();
                loc_t pos = pTermPositions->nextPosition();
                loc_t subpos = pTermPositions->nextPosition();
                while (pos != BAD_POSITION)
                {
                    if (decompressed_docid != docid)
                        newPosting->addLocation(decompressed_docid, pos, subpos);
                    pos = pTermPositions->nextPosition();
                    subpos = pTermPositions->nextPosition();
                }
                newPosting->updateDF(decompressed_docid);
            }

            InMemoryPosting* curPosting =  (InMemoryPosting*)postingMap_[termId];
            delete curPosting;
            postingMap_[termId] = newPosting;
        }
    }

    delete pTermReader;

}

void FieldIndexer::reset()
{
    InMemoryPosting* pPosting;
    for(InMemoryPostingMap::iterator iter = postingMap_.begin(); iter !=postingMap_.end(); ++iter)
    {
        pPosting = iter->second;
        if (!pPosting->hasNoChunk())
        {
            pPosting->reset();		///clear posting data
        }
    }
    postingMap_.clear();
}

fileoffset_t FieldIndexer::write(OutputDescriptor* pWriterDesc)
{
    vocFilePointer_ = pWriterDesc->getVocOutput()->getFilePointer();

    IndexOutput* pVocWriter = pWriterDesc->getVocOutput();

    fileoffset_t poffset;
    termid_t tid;
    fileoffset_t lastPOffset = 0;
    termid_t lastDocID = 0;
    int32_t termCount = 0;
    InMemoryPosting* pPosting;
    fileoffset_t vocOffset = pVocWriter->getFilePointer();

    for(InMemoryPostingMap::iterator iter = postingMap_.begin(); iter !=postingMap_.end(); ++iter)
    {
        pPosting = iter->second;
        if (!pPosting->hasNoChunk())
        {
            tid = iter->first;
            pVocWriter->writeInt(tid - lastDocID);			///write term id

            pVocWriter->writeInt(pPosting->docFreq());		///write df

            poffset = pPosting->write(pWriterDesc);		///write posting data

            pVocWriter->writeLong(poffset - lastPOffset);	///write offset of posting descriptor
            pPosting->reset();								///clear posting data

            lastDocID = tid;
            lastPOffset = poffset;

            termCount++;
        }
    }


    fileoffset_t vocDescOffset = pVocWriter->getFilePointer();
    int64_t vocLength = vocDescOffset - vocOffset;
    ///begin write vocabulary descriptor
    pVocWriter->writeLong(vocLength);	///<VocLength(Int64)>
    pVocWriter->writeLong(termCount);	///<TermCount(Int64)>
    ///end write vocabulary descriptor

    return vocDescOffset;
}

TermReader* FieldIndexer::termReader()
{
    return new InMemoryTermReader(getField(),this);
}
