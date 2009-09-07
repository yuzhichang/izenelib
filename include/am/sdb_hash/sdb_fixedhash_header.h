#ifndef SDB_FIXEDHASH_HEADER_H_
#define SDB_FIXEDHASH_HEADER_H_

#include <iostream>

using namespace std;

NS_IZENELIB_AM_BEGIN

/**
 * \brief FileHeader of sdb_hash
 *  
 */
struct SfhFileHeader {
	int magic; //set it as 0x061561, check consistence.			
	size_t keySize;
	size_t valueSize;
	size_t bucketSize;
	//size_t directorySize;
	size_t dpow;
	size_t cacheSize;
	size_t numItems;
	size_t nBlock; //the number of bucket allocated, it indicates the file size: sizeof(SfhFileHeader) + nblock*bucketSize

	SfhFileHeader()
	{
		keySize = 0;
		valueSize = 0;
		magic = 0x061561;
		bucketSize = 1024;
		//directorySize =8192*8;
		dpow = 20;
		cacheSize = 1<<(dpow-2);
		numItems = 0;
		nBlock = 0;
	}

	void display(std::ostream& os = std::cout) {
		os<<"magic: "<<magic<<endl;
		os<<"keySize: "<<keySize<<endl;
		os<<"valueSize: "<<valueSize<<endl;
		os<<"bucketSize: "<<bucketSize<<endl;
		os<<"dpow: "<<dpow<<endl;
		os<<"directorySize: "<<(1<<dpow)<<endl;
		os<<"cacheSize: "<<cacheSize<<endl;
		os<<"numItem: "<<numItems<<endl;
		os<<"nBlock: "<<nBlock<<endl;
		
		os<<endl;
		os<<"file size: "<<nBlock*bucketSize+sizeof(SfhFileHeader)<<"bytes"<<endl;
		if(nBlock != 0) {
			os<<"average items number in bucket: "<<double(numItems)/double(nBlock)<<endl;		
			os<<"average length of bucket chain: "<< double(nBlock)/double(1<<dpow)<<endl;
		}
	}

	bool toFile(FILE* f)
	{
		if ( 0 != fseek(f, 0, SEEK_SET) )
		return false;

		fwrite(this, sizeof(SfhFileHeader), 1, f);
		return true;

	}

	bool fromFile(FILE* f)
	{
		if ( 0 != fseek(f, 0, SEEK_SET) )
		return false;
		fread(this, sizeof(SfhFileHeader), 1, f);
		return true;
	}
};

NS_IZENELIB_AM_END

#endif /*SDB_HASH_HEADER_H_*/