#include <fstream>
#include <vector>

#include <boost/test/unit_test.hpp>
#include <wiselib/ustring/UString.h>

#include <sdb/SequentialDB.h>
#include <util/hashFunction.h>
#include <wiselib/ustring/UString.h>


using namespace std;
using namespace wiselib;
using namespace boost::unit_test;

namespace TestData
{

    bool loadTermList(const string& filePath, vector<string>& termList)

	{
    	char line[1024];
    	ifstream dictionaryFile(filePath.data());

    	if(!dictionaryFile.good() || !dictionaryFile.is_open())
        	return false;

    	termList.clear();
    	memset(line, 0, 1024);
    	dictionaryFile.getline(line, 1024);
    	while( !dictionaryFile.eof())
    	{
        	for(size_t i = 0; i < strlen(line); i++)
            	if(line[i] == ' ')
                	line[i] = 0;
        	// add word to the list
        	termList.push_back(line);

        	// read next line
        	memset(line, 0, 1024);
        	dictionaryFile.getline(line, 1024);
    	}
    	dictionaryFile.close();

    	return true;
	} // end - loadTermList

    /**
	 * @brief This function calls loadTermList to load a list of terms. The function also
	 * generates id for each term in the list. This function is implemented by TuanQuang Nguyen.
	 * @param
	 * 	termUStringList1 - list of the term string
	 */
	bool generateTermLists(vector<UString>& termUStringList1)
    {
        vector<string> termStringList1;

        // load term list
        if(!loadTermList("./test-data/100WordList.txt", termStringList1) )
            return false;

        // convert term list of strings into term list of UStrings
        termUStringList1.resize(termStringList1.size());
        for(size_t i = 0; i < termStringList1.size(); i++)
            termUStringList1[i].assign(termStringList1[i], UString::CP949);

        return true;

    } // end - generateTermLists()

}

BOOST_AUTO_TEST_SUITE( t_SDB)

BOOST_AUTO_TEST_CASE( TestCase1 )
{
    vector<unsigned int> keys;
    vector<UString> values;
    izenelib::sdb::ordered_sdb<unsigned int, UString, izenelib::util::NullLock> sdb("SDB1.sdb");
    sdb.open();
    TestData::generateTermLists(values);
    keys.resize(values.size());
    for(size_t i=0; i<values.size(); i++)
    {
        keys[i] = HashFunction<UString>::generateHash32(values[i]);
        sdb.insertValue(keys[i], values[i]);
    }
    for(size_t i=0; i<keys.size(); i++)
    {
        UString v;
        if( false == sdb.getValue(keys[i], v) )
            cerr << "find sdb err " << i << " th term: " << values[i] << " , ID" << keys[i] << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()