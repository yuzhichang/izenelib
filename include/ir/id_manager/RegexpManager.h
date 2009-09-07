/**
 * @author Wei Cao
 * @date 2009-09-05
 */

#ifndef _REGEXP_MANAGER_
#define _REGEXP_MANAGER_

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <types.h>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "LexicalTrie.h"
#include <am/sdb_trie_v1/sdb_trie.hpp>

NS_IZENELIB_IR_BEGIN

namespace idmanager {

/**
  * There are three kinds of regexp handlers selectable:
  * - Empty : if selected, nothing will RegexpManager do.
  * - All in memory version: based on LexicalTrie, every thing is kept in memory.
  * - Disk version: based on SDBTrie, data is swapped between memory and disk.
  */
template<typename NameString, typename NameID>
class BasicRegExpHandler
{
public:

	void openForRead() {}

	void openForWrite(){}

	void optimize(){}

	void close(){}

	void insert(const NameString & word, const NameID id){ }

	bool findRegExp(const NameString& exp, std::vector<NameID> & results){ return false;}

	int num_items(){return 0;}

	void display(){}
};

/**
 *@brief nothing will be done for any operation.
 */
template<typename NameString, typename NameID>
class EmptyRegExpHandler : public BasicRegExpHandler<NameString, NameID>
{
public:

	EmptyRegExpHandler(const std::string&){}

	void display(){std::cout << "This is a EmptyRegExpHandler instance" << std::endl; }

}; // end - class EmptyRegExpHandler

/**
 *@brief based on LexicalTrie, every thing is kept in memory.
 */
template<typename NameString, typename NameID>
class MemoryRegExpHandler : public BasicRegExpHandler<NameString, NameID>
{
public:

	MemoryRegExpHandler(const std::string&){}

	void insert(const NameString & word, const NameID id){ trie_.insert(word, id); }

	bool findRegExp(const NameString& exp, std::vector<NameID> & results){return trie_.findRegExp(exp, results);}

	int num_items(){return trie_.num_items();}

	void display(){std::cout << "This is a MemoryRegExpHandler instance" << std::endl; }

private:

    LexicalTrie<NameString, NameID> trie_;
}; // end - class MemoryRegExpHandler

/**
 *@brief based on SDBTrie, data is swapped between memory and disk.
 */
template<typename NameString, typename NameID>
class DiskRegExpHandler : public BasicRegExpHandler<NameString, NameID>
{
public:
	DiskRegExpHandler(const std::string& name)
	:   trie_(name) {}

	void openForRead() { trie_.openForRead(); }

	void openForWrite(){ trie_.openForWrite(); }

	void optimize(){ trie_.optimize(); }

	void close(){ trie_.close(); }

	void insert(const NameString & word, const NameID id){ trie_.insert(word, id); }

	bool findRegExp(const NameString& exp, std::vector<NameID> & results){return trie_.findRegExp(exp, results);}

	int num_items(){return trie_.num_items();}

	void display(){std::cout << "This is a DiskRegExpHandler instance" << std::endl; trie_.display(); }

private:

    SDBTrie2<NameString, NameID> trie_;
}; // end - class DiskRegExpHandler


/**
 * @brief A meta function to test if given class is an instantiation of template EmptyRegExpHandler.
 * @return true it's a EmptyRegExpHandler
 *         false otherwise
 */
template <typename RegExpHandler>
class IsEmpty {
public:
    static const bool value = false;
};
template <typename N,typename I>
class IsEmpty<EmptyRegExpHandler<N,I> > {
public:
    static const bool value = true;
};

/**
 * @brief A meta function to test if given class is an instantiation of template MemoryRegExpHandler.
 * @return true it's a MemoryRegExpHandler
 *         false otherwise
 */
template <typename RegExpHandler>
class IsMemory {
public:
    static const bool value = false;
};
template <typename N,typename I>
class IsMemory<MemoryRegExpHandler<N,I> > {
public:
    static const bool value = true;
};


/**
 * @brief Manager to handler regexp searches.
 */
template<typename NameString,
         typename NameID,
         typename RegExpHandler,
         typename LockType>
class RegexpManager
{
    typedef typename NameString::value_type CharType;

public:

    RegexpManager(const std::string& storageName)
        : storageName_(storageName),
          rawTextFileName_(storageName_ + ".rawtext"),
          rawIdFileName_(storageName_ + ".rawid"),
          handler_(NULL), worker_(NULL)
    {
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            rawTextFile_.open(rawTextFileName_.c_str(), std::ofstream::out|std::ofstream::app);
            if(rawTextFile_.fail())
                std::cerr << "bad file " << rawTextFileName_ << std::endl;

            rawIdFile_.open(rawIdFileName_.c_str(), std::ofstream::out|std::ofstream::app|std::ofstream::binary );
            if(rawIdFile_.fail())
                std::cerr << "bad file " << rawIdFileName_ << std::endl;
        }
    }

    ~RegexpManager()
    {
        close();
    }

	void insert(const NameString & word, const NameID & id)
	{
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            filelock_.acquire_write_lock();
            try {
                std::string str( (const char *)word.c_str(), word.size());
                rawTextFile_ << str <<"\n";
                rawIdFile_.write((char*)&id, sizeof(NameID));
            } catch(const std::exception & e) {
                std::cerr << "Files " << rawTextFileName_ << " and " << rawIdFileName_
                    << " locked or not exist, cause RegexpManager insert exception: "
                    << e.what() << std::endl;
            }
            filelock_.release_write_lock();
        }
    }

	bool findRegExp(const NameString& exp, std::vector<NameID> & results)
	{
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            if(handler_)
                return handler_->findRegExp(exp, results);

            filelock_.acquire_write_lock();
            rawTextFile_.flush();
            rawIdFile_.flush();
            /// TODO: use grep here


            filelock_.release_write_lock();
            return false;
        }
        return false;
    }

    void startThread(const unsigned int cacheSize)
    {
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            if(worker_) return;

            filelock_.acquire_write_lock();
            // Close write pipes
            rawTextFile_.flush();
            rawTextFile_.close();

            rawIdFile_.flush();
            rawIdFile_.close();
            filelock_.release_write_lock();

            worker_ = new boost::thread(boost::bind(&RegexpManager::workThread, this));
        }
    }

    void joinThread()
    {
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            if(worker_==NULL) return;

            worker_->join();
            delete worker_;
            worker_ = NULL;

            filelock_.acquire_write_lock();
            // Reopen write pipes
            rawTextFile_.open(rawTextFileName_.c_str(), std::ofstream::out|std::ofstream::app);
            if(rawTextFile_.fail())
                std::cerr << "bad file " << rawTextFileName_ << std::endl;
            rawIdFile_.open(rawIdFileName_.c_str(), std::ofstream::out|std::ofstream::app|std::ofstream::binary );
            if(rawIdFile_.fail())
                std::cerr << "bad file " << rawIdFileName_ << std::endl;
            filelock_.release_write_lock();
        }
    }

    void close()
    {
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {

            filelock_.acquire_write_lock();
            // Close write pipes
            rawTextFile_.flush();
            rawTextFile_.close();

            rawIdFile_.flush();
            rawIdFile_.close();
            filelock_.release_write_lock();

            if(handler_) {
                handler_->close();
                delete handler_;
                handler_ = NULL;
                handlerInitialized_ = false;
            }

            if(worker_) {
                worker_->join();
                delete worker_;
                worker_ = NULL;
            }
        }
    }

protected:

    void workThread()
    {
        // here if statement could be optimized at compile time,
        // so it doesn't effect the perforamnce.
        if( ! IsEmpty<RegExpHandler>::value )
        {
            // Open read pipes
            ifstream tin, iin;
            tin.open(rawTextFileName_.c_str(), std::ifstream::in);
            iin.open(rawIdFileName_.c_str(), std::ifstream::in|std::ifstream::binary);
            if (tin.fail() || iin.fail() )
            {
                std::cout<<"Can't open raw files "<< rawTextFileName_
                    << " and " << rawIdFileName_ << std::endl;
                return;
            }

            if(handler_) {
                handler_->close();
                delete handler_;
            }

            handler_ = new RegExpHandler(storageName_);
            // write in Trie
            handler_->openForWrite();
            int skip = handler_->num_items();

            int line = 0;
            std::string buffer;
            NameString key;
            NameID id;
            while(getline(tin, buffer))
            {
                // skip records already in Trie
                if( line >= skip )
                {
                    key = (CharType*)buffer.c_str();
                    iin.read((char*)&id, sizeof(NameID));
                    handler_->insert(key, id);
                }
                line++;
            }
            handler_->optimize();

            // if this is a disk version regexp handler
            // build a trie and flush it into disk
            if(false == IsMemory<RegExpHandler>::value)
            {
                handler_->close();
                delete handler_;
                handler_ = new RegExpHandler(storageName_);
                handler_->openForRead();
            }
        }
    }

private:

    std::string storageName_;

    std::string rawTextFileName_;

    std::ofstream rawTextFile_;

    std::string rawIdFileName_;

    std::ofstream rawIdFile_;

    bool handlerInitialized_;

    LockType filelock_;

    RegExpHandler* handler_;

    boost::thread* worker_;
};

}

NS_IZENELIB_IR_END

#endif