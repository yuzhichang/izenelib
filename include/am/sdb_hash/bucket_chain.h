/**
 * @file bucket_chain.h
 * @brief The header file of bucket_chain.
 * @author peisheng wang * 
 *
 * This file defines class bucket_chain.
 */

#ifndef bucket_chain_H_
#define bucket_chain_H_


#include <cstdlib>
#include <cstddef>
#include <fstream>
#include <iostream>

using namespace std;
/**
 *  \brief bucket_chain, represents a bucket of our array hash. 
 *   
 *   It has fixed size and an pointer to next bucket_chain element.
 *   It uses a char* member str to store item(binary) sequentially. When full, items will 
 *   be stored to next bucket_chain element.   
 *
 */
class bucket_chain {	
	size_t bucketSize_;
public:
	char *str;
	long fpos;
	int num;
	bucket_chain* next;
	bool isLoaded;
	bool isDirty;	
	
	long nextfpos;
	
	int level;
	
	//It indicates how many active bucket_chains in memory.
	//It increases when allocateBlock() called, or reading from disk,
	//and decreases when unload() called. 
	static size_t activeNum;

public:
	/**
	 *  constructor
	 */
	bucket_chain(size_t bucketSize) :
	bucketSize_(bucketSize) {
		str = NULL;
		num = 0;
		next = 0;
		isLoaded = false;
		isDirty = true;
		fpos = 0;
		nextfpos = 0;
		
		level = 0;				
		//++activeNum;
	}
    
	/**	 
	 *  deconstructor
	 */
	virtual ~bucket_chain() {
		if( str ){
			delete str;
			str = 0;
		}					
		isLoaded = false;
		//--activeNum;
	}

	/**
	 *  write to disk
	 */
	bool write(FILE* f) {
		if (!isDirty) {
			return true;
		}

		if (!isLoaded) {
			return true;
		}

		if (!f) {
			return false;
		}

		//cout<<"write fpos="<<fpos<<endl;
		if ( 0 != fseek(f, fpos, SEEK_SET) )
		return false;

		if (1 != fwrite(&num, sizeof(int), 1, f) ) {
			return false;
		}
		//cout<<"write num="<<num<<endl;

		size_t blockSize = bucketSize_ - sizeof(int) - sizeof(long);
			
		if (1 != fwrite(str, blockSize, 1, f) ) {
			return false;
		}

		//long nextfpos = 0;

		if (next)
		nextfpos = next->fpos;

		//cout<<"write nextfpos = "<< nextfpos<<endl;
		if (1 != fwrite(&nextfpos, sizeof(long), 1, f) ) {
			return false;
		}

		isDirty = false;
		isLoaded = true;

		return true;
	}
	

	/**
	 *  read from disk
	 */
	bool read(FILE* f) {
		if (!f) {
			return false;
		}

		//cout<<"read from "<<fpos<<endl;
		if ( 0 != fseek(f, fpos, SEEK_SET) )
		return false;

		if (1 != fread(&num, sizeof(int), 1, f) ) {
			return false;
		}
		
		//cout<<"read num="<<num<<endl;
		size_t blockSize = bucketSize_ - sizeof(int) - sizeof(long);
		
		if ( !str )
		{
			str = new char[blockSize];		
     	    memset(str, 0, blockSize);
		}
 
		//cout<<"read blocksize="<<blockSize<<endl;
		if (1 != fread(str, blockSize, 1, f) ) {
			return false;
		}

		//long nextfpos = 0;

		if (1 != fread(&nextfpos, sizeof(long), 1, f) ) {
			return false;
		}

		//cout<<"read next fpos="<<nextfpos<<endl;
		if (nextfpos !=0) {
			if( !next )next = new bucket_chain(bucketSize_);
			next->fpos = nextfpos;
		}
		isLoaded = true;
		isDirty = false;		
				
		++activeNum;
		
		return true;
	}

	/**
	 *    load next bucket_chain element
	 */ 
	bucket_chain* loadNext(FILE* f) {		
		if (next && !next->isLoaded) {	
			//cout<<"reading next"<<endl;
			next->read(f);						
		}		
		if( next )next->level = level+1;
		return next;
	}
	
	/**
	 *   unload a buck_chain element.
	 *   It releases most of the memory, and was used to recycle memory when cache is full. 
	 */
	void unload(){			
		if(str){
			delete str;
			str = 0;			
			--activeNum;			
		}	
		isLoaded = false;
		
		//cout<<"unload fpos="<<fpos<<endl;
		//cout<<"activeNode: "<<activeNum<<endl;
	}

	/**
	 *   display string_chain info.
	 */
	void display(std::ostream& os = std::cout) {		
		os<<"(level: "<<level;
		os<<"  isLoaded: "<<isLoaded;
		os<<"  bucketSize: "<<bucketSize_;
		os<<"  numitems: "<<num;
		os<<"  fpos: "<<fpos;
		if(next)os<<"  nextfpos: "<<next->fpos;
		//os<<"str: "<<str;
		os<<")- > ";		
		if (next)
			next->display(os);
	}
};


#endif /*bucket_chain_H_*/