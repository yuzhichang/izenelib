/**
 * @brief A set of mock objects for replacing IndexManager in unitests.
 * @author Wei Cao
 * @date 2009-10-14
 */

#ifndef _MOCK_INDEXREADER_H_
#define _MOCK_INDEXREADER_H

#include <string>
#include <strstream>
#include <vector>
#include <map>

#include <boost/tuple/tuple.hpp>

#include <ir/index_manager/utility/system.h>
#include <ir/index_manager/index/Term.h>
#include <ir/index_manager/store/Directory.h>
#include <ir/index_manager/index/TermInfo.h>
#include <ir/index_manager/index/FieldInfo.h>
#include <ir/index_manager/index/CollectionInfo.h>
#include <ir/index_manager/index/FieldIndexer.h>
#include <ir/index_manager/index/ForwardIndexReader.h>

NS_IZENELIB_IR_BEGIN

namespace indexmanager {

class MockTermReader;
class MockTermIterator;
class MockTermDocFreqs;
class MockTermPositions;

/**
 * Mock object for IndexReader
 */
class MockIndexReaderWriter
{
friend class MockTermReader;
friend class MockTermIterator;
friend class MockTermDocFreqs;
friend class MockTermPositions;

public:
    MockIndexReaderWriter();

    ~MockIndexReaderWriter();

    count_t numDocs() { return forward_.size(); }

    count_t maxDoc() { return forward_.lower_bound((docid_t)-1)->first; }

    freq_t docFreq(collectionid_t colID, Term* term);

    TermInfo* termInfo(collectionid_t colID, Term* term);

    /**
     * @param colID ignored
     */
    MockTermReader* getTermReader(collectionid_t colID);

//    /// Not Implemented yet
//    MockForwardIndexReader* getForwardIndexReader(){
//        return NULL;
//    }

    /**
     * @param terms write space separated termid_t list
     */
    bool insertDoc(docid_t docid ,
            std::string& property, std::string& terms)
    {
        if ( terms.size() == 0 ) return false;

        std::vector<termid_t> tl;
        termid_t t;

        std::stringstream ss(terms);
        while(!ss.eof()) {
            ss >> t;
            tl.push_back(t);
        }
        return insertDoc(docid, property, tl);
    }

    bool insertDoc(docid_t docid ,
            std::string& property, std::vector<termid_t>& terms)
    {
        if( !forward_.insert( std::make_pair(docid, std::make_pair(property, terms)) ).second )
            return false;

        for(size_t i  =0; i< terms.size(); i++ ) {
            // get postings
            std::pair<std::string, termid_t> term = std::make_pair(property, terms[i]);
            std::vector<boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > >& postings = inverted_[term];

            // find posting
            typedef std::vector<boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > >::iterator Iter;
            Iter it = postings.begin();
            for( ; it != postings.end(); it ++ ) {
                if( boost::get<0>(*it) <= docid )
                    break;
            }

            // update posting
            if( boost::get<0>(*it) == docid ) {
                boost::get<1>(*it) ++;
                boost::get<3>(*it).push_back(i);
            } else {
                // insert new posting
                boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > posting;
                boost::get<0>(posting) = docid;
                boost::get<1>(posting) = 1;
                boost::get<2>(posting) = terms.size();
                boost::get<3>(posting).push_back(i);
                postings.insert(it, posting);
            }
        }
        return true;
    }

private:

    std::map<docid_t, std::pair<std::string, std::vector<termid_t> > > forward_;

    /// <field, term> --> <docid, tf, docLen, positions>
    std::map<std::pair<std::string, termid_t>,
        std::vector<boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > > > inverted_;

    MockTermReader* reader_;

};

class MockTermReader {

public:

    MockTermReader(MockIndexReaderWriter* index);

    ~MockTermReader(void);

    /// Nothing to do
    void open(Directory* pDirectory,const char* barrelname,FieldInfo* pFieldInfo){}

    /// Nothing to do
    void close(){}

    MockTermIterator* termIterator(const char* field);
    /**
    * find the term in the vocabulary,return false if not found
    */
    bool seek(Term* pTerm) {
        if(!term_ || term_->compare(pTerm)) {
            if(!term_ ) {
                term_ = new Term(*pTerm);
            } else {
                term_->setField(pTerm->getField());
                term_->setValue(pTerm->getValue());
            }
            if( termInfo(term_) ) return true;
            return false;
        }
    }

    MockTermDocFreqs*	termDocFreqs();

    MockTermPositions*	termPositions();

    freq_t docFreq(Term* term) {
        TermInfo* ti = termInfo(term);
        if(ti) return ti->docFreq();
        return 0;
    }

    TermInfo* termInfo(Term* term);

    MockTermReader*	clone() {
        return new MockTermReader(index_);
    }

private:

    MockIndexReaderWriter* index_;

    Term* term_;
    TermInfo* termInfo_;
};

/// Mock object for TermIterator
class MockTermIterator {
public:
    MockTermIterator(MockIndexReaderWriter* index, std::string property);

    ~MockTermIterator();

public:
    bool next();

    const Term* term() {
        term_.setField(cursor_->first.first.c_str());
        term_.setValue(cursor_->first.second);
        return &term_;
    }

    const TermInfo* termInfo() {
        termInfo_.set(cursor_->second.size(), -1);
        return &termInfo_;
    };

private:
    MockIndexReaderWriter* index_;
    std::string property_;

    std::map<std::pair<std::string, termid_t>,
        std::vector<boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > > > ::iterator cursor_;

    Term term_;
    TermInfo termInfo_;
};

/// Mock object for TermDocFreqs
class MockTermDocFreqs {
public:
    MockTermDocFreqs(MockIndexReaderWriter* index, Term& term);

    virtual ~MockTermDocFreqs() {}

    freq_t	docFreq() { return postings_.size(); }

    int64_t	getCTF() { return ctf_;}

    virtual count_t next(docid_t*& docs, count_t*& freqs) {
        if(cursor_ == postings_.size()) return 0;
        docs = &boost::get<0>(postings_[cursor_]);
        freqs = &boost::get<1>(postings_[cursor_]);
        cursor_++;
        return 1;
    }

    virtual bool next() {
        if(cursor_ == postings_.size()) return false;
        cursor_++; return true;
    }

    docid_t doc() { return boost::get<0>(postings_[cursor_]); }

    count_t freq() { return boost::get<1>(postings_[cursor_]); }

    freq_t docLength() { return boost::get<2>(postings_[cursor_]); }

    virtual void close(){ postings_.clear(); cursor_ = 0; }

protected:
    MockIndexReaderWriter* index_;
    std::string property_;
    termid_t termid_;

    std::vector<boost::tuple<docid_t, count_t, freq_t, std::vector<loc_t> > > postings_;
    int64_t ctf_;
    size_t cursor_;
};

/// Mock object of TermPostions
class MockTermPositions : public MockTermDocFreqs {
public:
    MockTermPositions(MockIndexReaderWriter* index, Term& term);

    ~MockTermPositions(){}

    count_t next(docid_t*& docs, count_t*& freqs) {
        posCursor_ = 0;
        return MockTermDocFreqs::next(docs, freqs);
    }

    bool next() {
        posCursor_ = 0;
        return MockTermDocFreqs::next();
    }

    loc_t nextPosition()
    {
        loc_t pos = BAD_POSITION;
        if( posCursor_ < boost::get<3>(postings_[cursor_]).size() ) {
            pos = boost::get<3>(postings_[cursor_])[posCursor_];
            posCursor_++;
        }
        return pos;
    }

    int32_t nextPositions(loc_t*& positions)
    {
        if( posCursor_ < boost::get<3>(postings_[cursor_]).size() ) {
            positions = &boost::get<3>(postings_[cursor_])[posCursor_];
            posCursor_++;
            return 1;
        }
        return 0;
    }

protected:
    size_t posCursor_;
};

}

NS_IZENELIB_IR_END

#endif
