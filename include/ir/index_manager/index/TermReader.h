/**
* @file        TermReader.h
* @author     Yingfeng Zhang
* @version     SF1 v5.0
* @brief Multi Term Reader
*/

#ifndef TERMREADER_H
#define TERMREADER_H

#include <string>

#include <ir/index_manager/utility/system.h>
#include <ir/index_manager/index/Term.h>
#include <ir/index_manager/index/TermDocFreqs.h>
#include <ir/index_manager/index/TermPositions.h>
#include <ir/index_manager/index/TermInfo.h>
#include <ir/index_manager/index/TermIterator.h>
#include <ir/index_manager/index/FieldInfo.h>
#include <ir/index_manager/store/Directory.h>


NS_IZENELIB_IR_BEGIN

namespace indexmanager{
class TermIterator;
class TermDocFreqs;
class TermPositions;
/**
* Base class of InMemoryTermReader and DiskTermReader
*/
class TermReader
{
public:
    TermReader(void);
    TermReader(FieldInfo* pFieldInfo_);
    virtual ~TermReader(void);
public:
    /**
    * open a index barrel
    */
    virtual void open(Directory* pDirectory,const char* barrelname,FieldInfo* pFieldInfo);

    virtual TermIterator* termIterator(const char* field) = 0;
    /**
    * find the term in the vocabulary,return false if not found
    */
    virtual bool seek(Term* pTerm) = 0;

    virtual TermDocFreqs*	termDocFreqs() = 0;

    virtual TermPositions*	termPositions() = 0;

    virtual freq_t docFreq(Term* term) = 0;

    virtual void close() = 0;
    /**
     * clone the term reader
     * @return term reader, MUST be deleted by caller.
     */
    virtual TermReader*	clone() = 0;

protected:
    virtual TermInfo* termInfo(Term* term)
    {
        return NULL;
    };
public:
    FieldInfo* getFieldInfo()
    {
        return pFieldInfo;
    }

    void setFieldInfo(FieldInfo* pFieldInfo)
    {
        this->pFieldInfo = pFieldInfo;
    }
protected:
    FieldInfo* pFieldInfo;	///reference to field info

    friend class TermDocFreqs;
    friend class MultiFieldTermReader;
    friend class IndexReader;
    friend class InMemoryTermReader;
};

struct TERM_TABLE
{
    termid_t tid;
    TermInfo ti;
};
/**
* Internal class of DiskTermReader
* We use this class because there would exist concurrent read, without this class,
* we have to repeat constructing the vocabulary in memory when read the index concurrently
*/

class TermReaderImpl
{
public:
    TermReaderImpl(FieldInfo* pFieldInfo_);

    ~TermReaderImpl();
public:

    /**
     * open a index barrel
     * @param pDirectory index storage
     * @param barrelname index barrel name
     * @param pFieldInfo field information
     */
    void open(Directory* pDirectory,const char* barrelname,FieldInfo* pFieldInfo);

    bool seek(Term* pTerm);

    void close() ;

    void updateTermInfo(Term* term, count_t docFreq, fileoffset_t offset);

    TermInfo* termInfo(Term* term);

public:
    FieldInfo* pFieldInfo;

    TERM_TABLE* pTermTable;

    InputDescriptor* pInputDescriptor;

    int32_t nTermCount;

    int64_t nVocLength;
};

class DiskTermReader:public TermReader
{
public:
    DiskTermReader();

    DiskTermReader(TermReaderImpl* pTermReaderImpl);

    virtual ~DiskTermReader(void);
public:
    /**
     * open a index barrel
     * @param pDirectory index storage
     * @param barrelname index barrel name
     * @param pFieldInfo field information
     */
    void open(Directory* pDirectory,const char* barrelname,FieldInfo* pFieldInfo);



    /**
     * get the term iterator
     * @param field field name
     * @return term iterator, MUST be deleted by caller
     */
    TermIterator* termIterator(const char* field);


    bool seek(Term* pTerm);


    TERM_TABLE* getTermTable()
    {
        return pTermReaderImpl->pTermTable;
    }

    /**
     * get term's document postings,must be called after calling {@link seek()} success
     * @return return document postings,need to be deleted outside
     */
    TermDocFreqs* termDocFreqs();

    /**
     * get term's position postings,must be called after calling {@link seek()} success
     * @return return position postings,need to be deleted outside
     */
    TermPositions* termPositions();

    /**
     * get document frequency of a term
     * @return document frequency
     */
    freq_t docFreq(Term* term);


    /**
     * close term reader
     */
    void close() ;

    /**
     * clone the term reader
     * @return term reader, MUST be deleted by caller.
     */
    TermReader* clone() ;

    void updateTermInfo(Term* term, count_t docFreq, fileoffset_t offset);

    TermReaderImpl* getTermReaderImpl()
    {
        return pTermReaderImpl;
    }

protected:
    /**
     * get term information of a term
     * @return term information
     */
    TermInfo* termInfo(Term* term);

protected:
    TermReaderImpl* pTermReaderImpl;

    TermInfo* pCurTermInfo;

    bool ownTermReaderImpl;

    friend class DiskTermIterator;
    friend class CollectionIndexer;

};

class InMemoryTermReader : public TermReader
{
public:
    InMemoryTermReader(void);

    InMemoryTermReader(const char* field,FieldIndexer* pIndexer);

    virtual ~InMemoryTermReader(void);
public:
    void open(Directory* pDirectory,const char* barrelname,FieldInfo* pFieldInfo);
    /**
     * get the term iterator
     * @param pLowerTerm lower bound
     * @param pUpperTerm upper bound
     * @return term iterator, MUST be deleted by caller
     */
    TermIterator* termIterator(Term* pLowerTerm,Term* pUpperTerm);

    /**
     * get the term iterator
     * @param field field name
     * @return term iterator, MUST be deleted by caller
     */
    TermIterator* termIterator(const char* field);

    /**
     * seek a term
     * @param pTerm term
     * @return true if success, otherwise false, if the return value is true then {@link termDocFreqs()} and
     * {@link termPositions()}can be called.
     */
    bool seek(Term* term);

    /**
     * get term's document postings,must be called after call seek() success
     * @return return document postings,need to be deleted outside
     */
    TermDocFreqs* termDocFreqs();

    /**
     * get term's position postings,must be called after call seek() success
     * @return return position postings,need to be deleted outside
     */
    TermPositions* termPositions();

    freq_t docFreq(Term* term);

    void close();

    /**
     * clone the term reader
     * @return term reader, MUST be deleted by caller.
     */
    TermReader* clone() ;

public:
    TermInfo* termInfo(Term* term);
    /**
     * get in-memory posting
     * @return reference to in-memory posting
     */
    InMemoryPosting* inMemoryPosting();
protected:
    string sField;

    FieldIndexer* pIndexer;

    TermInfo* pCurTermInfo;

    InMemoryPosting* pCurPosting;

    TermInfo* pTermInfo;

    friend class InMemoryTermIterator;
};


}

NS_IZENELIB_IR_END

#endif