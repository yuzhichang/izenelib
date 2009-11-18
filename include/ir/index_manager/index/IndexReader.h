/**
* @file        IndexReader.h
* @author     Yingfeng Zhang
* @version     SF1 v5.0
* @brief Index Reader
*/
#ifndef INDEXREADER_H
#define INDEXREADER_H

#include <boost/thread.hpp>

#include <ir/index_manager/utility/system.h>
#include <ir/index_manager/store/Directory.h>
#include <ir/index_manager/index/IndexBarrelReader.h>
#include <ir/index_manager/index/TermInfo.h>
#include <ir/index_manager/index/BarrelInfo.h>
#include <ir/index_manager/index/CollectionInfo.h>
#include <ir/index_manager/index/ForwardIndexReader.h>
#include <ir/index_manager/index/IndexerDocument.h>
#include <ir/index_manager/index/DocLengthReader.h>
#include <ir/index_manager/utility/BitVector.h>

NS_IZENELIB_IR_BEGIN

namespace indexmanager{

class Indexer;
class Term;

class IndexReader
{
private:
    IndexReader(Indexer* pIndex);
public:
    virtual ~IndexReader();
public:
    count_t numDocs();

    docid_t maxDoc();

    size_t docLength(docid_t docId, fieldid_t fid);

    double getAveragePropertyLength(fieldid_t fid);

    freq_t docFreq(collectionid_t colID, Term* term);

    TermInfo* termInfo(collectionid_t colID, Term* term);

    BarrelsInfo* getBarrelsInfo();

    static int64_t lastModified(Directory* pDirectory);

    ///client must delete the returned object
    TermReader* getTermReader(collectionid_t colID);

    ///client must delete the returned object
    ForwardIndexReader* getForwardIndexReader();

    void deleteDocumentPhysically(IndexerDocument* pDoc);
    /// mark a document as deleted;
    void delDocument(collectionid_t colID, docid_t docId);

    size_t getDistinctNumTerms(collectionid_t colID, const std::string& property);

    BitVector* getDocFilter() { return pDocFilter_; }

    void delDocFilter();

    void reopen();
private:
    void createBarrelReader();

    TermReader* doGetTermReader_(collectionid_t colID);
    ///find which barrel the document lies
    BarrelInfo* findDocumentInBarrels(collectionid_t colID, docid_t docID);

    void reload();
private:
    Indexer* pIndexer_; ///reference to index object

    BarrelsInfo* pBarrelsInfo_; ///reference to Index's pBarrelsInfo

    IndexBarrelReader* pBarrelReader_; ///barrel reader

    ForwardIndexReader* pForwardIndexReader_;

    mutable boost::mutex mutex_;

    BitVector* pDocFilter_;

    DocLengthReader* pDocLengthReader_;

    friend class Indexer;
};
inline BarrelsInfo* IndexReader::getBarrelsInfo()
{
    return pBarrelsInfo_;
}

}

NS_IZENELIB_IR_END

#endif
