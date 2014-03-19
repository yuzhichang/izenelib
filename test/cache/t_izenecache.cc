#ifndef T_IZENECACHE_CPP_
#define T_IZENECACHE_CPP_

#include <cstdio>
#include <cstdlib>
#include <cache/IzeneCache.h>
#include <cache/concurrent_cache.hpp>
#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <util/ustring/UString.h>
#include <glog/logging.h>


namespace rde{

/*
template<>
inline  hash_value_t extract_int_key_value<izenelib::util::UString>(const izenelib::util::UString& t){
    uint64_t convkey = 0;

    char* p = (char*)t.c_str();
    izenelib::util::UString::size_t length = t.length();
    size_t size = length*sizeof(izenelib::util::UString::value_type);
    for (size_t i = 0; i < size; i++)
        convkey = 37*convkey + (uint8_t)*p++;
    return convkey;
}
*/

}


using namespace std;
using namespace izenelib::am;
using namespace izenelib::cache;
using namespace izenelib::concurrent_cache;

typedef string KeyType;
typedef int ValueType;
//typedef izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, RDE_HASH, LFU> MyCache;
typedef ILRUCache<KeyType, ValueType> MyCache;

static string inputFile("test.txt");
static string inputFile1("test1.txt");
static bool trace = true;
static size_t cacheSize = 2501;

void get_cache_func(ConcurrentCache<int, int>* con_cache)
{
    int32_t cache_hit = 0;
    int32_t cnt = 10;
    int ret = 0;
    while(cnt-- > 0)
    {
        for (size_t i = 0; i < 1000000; ++i)
        {
            if( con_cache->get(i, ret) )
                cache_hit++;
        }
        //BOOST_CHECK(con_cache->check_correctness());
    }

    LOG(INFO) << con_cache->get_useful_info();
}

void write_cache_func(ConcurrentCache<int, int>* con_cache)
{
    int32_t cache_full = 0;
    int32_t cnt = 2;
    while(cnt-- > 0)
    {
        for (size_t i = 0; i < 1000000; ++i)
        {
            if( !con_cache->insert(i, i) )
            {
                //BOOST_CHECK(con_cache->check_correctness());
                cache_full++;
                usleep(100);
            }
        }
        //BOOST_CHECK(con_cache->check_correctness());
    }
    LOG(INFO) << "cache full : " << cache_full;
}

BOOST_AUTO_TEST_SUITE( izene_cache_suite )

BOOST_AUTO_TEST_CASE(izene_concurrent_cache_test)
{
    using namespace izenelib::concurrent_cache;
    static const std::size_t cache_size = 100;
    static const double wash_out_threshold = 0.1;
    {
        // single thread common unit test.
        ConcurrentCache<std::string, std::string> con_cache(cache_size, LRU, 10, wash_out_threshold);
        for(size_t i = 0; i < cache_size/2; ++i)
        {
            std::string tmp = boost::lexical_cast<std::string>(i);
            con_cache.insert(tmp, tmp);
            std::string ret;
            con_cache.get(tmp, ret);
            BOOST_CHECK_EQUAL(ret, tmp);
            BOOST_CHECK_EQUAL(con_cache.get(tmp + "1", ret), false);
            usleep(10);
        }
        BOOST_CHECK(con_cache.check_correctness());
        for(size_t i = 0; i < cache_size/4; ++i)
        {
            std::string tmp = boost::lexical_cast<std::string>(i);
            con_cache.remove(tmp);
            std::string ret;
            BOOST_CHECK_EQUAL(con_cache.get(tmp, ret), false);
        }
        BOOST_CHECK(con_cache.check_correctness());
        con_cache.clear();
        BOOST_CHECK(con_cache.check_correctness());
        for(size_t i = 0; i < cache_size/2; ++i)
        {
            std::string tmp = boost::lexical_cast<std::string>(i);
            std::string ret;
            BOOST_CHECK_EQUAL(con_cache.get(tmp, ret), false);
        }
        BOOST_CHECK(con_cache.check_correctness());

        std::string insert_failed;
        for(size_t i = 0; i < cache_size * 2 ; ++i)
        {
            std::string tmp = boost::lexical_cast<std::string>(i);
            if (!con_cache.insert(tmp, tmp))
            {
                LOG(INFO) << "cache insert full:" << i;
                insert_failed = tmp;
                break;
            }
            usleep(10000);
        }
        BOOST_CHECK(con_cache.check_correctness());
        sleep(10);
        BOOST_CHECK_EQUAL(con_cache.insert(insert_failed, insert_failed), true);
        BOOST_CHECK(con_cache.check_correctness());
    }
    {
        ConcurrentCache<int, int> con_cache(cache_size*10000, LRLFU, 1, wash_out_threshold);
        boost::thread_group read_group;
        boost::thread_group write_group;
        LOG(INFO) << "begin perf test for concurrent cache.";
        for(int i = 0; i < 16; ++i)
        {
            write_group.add_thread(new boost::thread(boost::bind(&write_cache_func, &con_cache)));
        }
        for(int i = 0; i < 16; ++i)
        {
            read_group.add_thread(new boost::thread(boost::bind(&get_cache_func, &con_cache)));
        }
        BOOST_CHECK(con_cache.check_correctness());
        read_group.join_all();
        BOOST_CHECK(con_cache.check_correctness());
        LOG(INFO) << "all cache read perf thread finished.";
        write_group.join_all();
        BOOST_CHECK(con_cache.check_correctness());
    }
}

BOOST_AUTO_TEST_CASE(izene_cache_test)
{
    trace = 0;
    MyCache cm(cacheSize);
    {
        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (trace) {
                cout<< "Insert: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
            if (cm.insertValue(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            //cm.displayCacheInfos();
        }

        cout<<"MCache with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;

        unsigned long vm = 0, rss = 0;
        ProcMemInfo::getProcMemInfo(vm, rss);

        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;
        double hitRatio, workload;
        cm.getEfficiency(hitRatio, workload);
        cout<<"\nTesting GetEffclassicency:"<<endl;
        cout<<"HitRatio: "<<hitRatio<<endl;
        cout<<"workload: "<<workload<<endl;
        cm.resetStartingTime();
    }

    {
        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            if (cm.getValue(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
            //cm.displayCacheInfos();
        }

        cout<<"MCache with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;

    }

    {
        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"MCache with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;

    }

    {
        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile.c_str());
        string ystr;
        ValueType val = 100;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.updateValue(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cm.getValue(ystr, val);
                cout<< "update: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"MCache with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;

    }

    {
        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile.c_str());
        string ystr;
        ValueType val;
        while (inf>>ystr) {
            sum++;
            if (cm.del(ystr))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            assert( cm.getValue(ystr, val) == false );
            if (trace) {
                cout<< "del: key="<<ystr<<"->"<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"MCache with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
    }
}

BOOST_AUTO_TEST_CASE(izene_cache_performance_test)
{
    cout<<"\n=================LRU==================="<<endl;
    trace = 0;
    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, RDE_HASH, LRU> cm(cacheSize*10);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH with "<<"CacheSize="<<cacheSize*10<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, LINEAR_HASH, LRU> cm(cacheSize*10);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"LINEAR_HASH with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    /*{
      izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, STX_BTREE> cm(cacheSize*10);

      int sum =0;
      int hit =0;
      clock_t t1 = clock();
      ifstream inf(inputFile1.c_str());
      string ystr;
      ValueType val = 0;
      while (inf>>ystr) {
      sum++;
      val++;
      if (cm.getValueWithInsert(ystr, val))
      hit++;
      else {
    //	outf<<ystr<<endl;
    }
    if (trace) {
    cout<< "get: "<<ystr<<" -> "<<val<<endl;
    cout<< "MCache numItem = "<<cm.numItems()<<endl;
    cm.displayHash();
    cout<<endl;
    }
    }

    cout<<"STX_BTREE with "<<"CacheSize="<<cacheSize<<endl;
    cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
    cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
    unsigned long vm = 0, rss = 0;

    ProcMemInfo::getProcMemInfo(vm, rss);

    //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
    cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }*/

    /*	{
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, CCCR_HASH> cm(cacheSize*10);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
        sum++;
        val++;
        if (cm.getValueWithInsert(ystr, val))
        hit++;
        else {
    //	outf<<ystr<<endl;
    }
    if (trace) {
    cout<< "get: key="<<ystr<<"->"<<val<<endl;
    cout<< "MCache numItem = "<<cm.numItems()<<endl;
    cm.displayHash();
    cout<<endl;
    }
    }

    cout<<"CCCR_HASH with "<<"CacheSize="<<cacheSize<<endl;
    cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
    cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
    unsigned long vm = 0, rss = 0;

    ProcMemInfo::getProcMemInfo(vm, rss);

    //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
    cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }*/

    trace = 0;
    {
        izenelib::cache::IzeneCache<izenelib::util::UString, ValueType, NullLock, RDE_HASH, LRU> cm(cacheSize*10);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        izenelib::util::UString ystr;
        string str;
        ValueType val = 0;
        while (inf>>str) {
            sum++;
            val++;
            ystr = izenelib::util::UString(str, izenelib::util::UString::CP949);
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<str<<"->"<<val<<endl;
                cout<< "izeneCache (UString->int) numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH(UString->int) with "<<"CacheSize="<<cacheSize*10<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    cout<<"\n=================LFU==================="<<endl;
    trace = 0;
    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, RDE_HASH, LFU> cm(cacheSize*2);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH with "<<"CacheSize="<<cacheSize*2<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, LINEAR_HASH, LFU> cm(cacheSize*2);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"LINEAR_HASH with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    /*{
      izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, STX_BTREE> cm(cacheSize*2);

      int sum =0;
      int hit =0;
      clock_t t1 = clock();
      ifstream inf(inputFile1.c_str());
      string ystr;
      ValueType val = 0;
      while (inf>>ystr) {
      sum++;
      val++;
      if (cm.getValueWithInsert(ystr, val))
      hit++;
      else {
    //	outf<<ystr<<endl;
    }
    if (trace) {
    cout<< "get: "<<ystr<<" -> "<<val<<endl;
    cout<< "MCache numItem = "<<cm.numItems()<<endl;
    cm.displayHash();
    cout<<endl;
    }
    }

    cout<<"STX_BTREE with "<<"CacheSize="<<cacheSize<<endl;
    cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
    cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
    unsigned long vm = 0, rss = 0;

    ProcMemInfo::getProcMemInfo(vm, rss);

    //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
    cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }*/

    /*	{
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, CCCR_HASH> cm(cacheSize*2);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
        sum++;
        val++;
        if (cm.getValueWithInsert(ystr, val))
        hit++;
        else {
    //	outf<<ystr<<endl;
    }
    if (trace) {
    cout<< "get: key="<<ystr<<"->"<<val<<endl;
    cout<< "MCache numItem = "<<cm.numItems()<<endl;
    cm.displayHash();
    cout<<endl;
    }
    }

    cout<<"CCCR_HASH with "<<"CacheSize="<<cacheSize<<endl;
    cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
    cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
    unsigned long vm = 0, rss = 0;

    ProcMemInfo::getProcMemInfo(vm, rss);

    //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
    cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }*/

    trace = 0;
    {
        izenelib::cache::IzeneCache<izenelib::util::UString, ValueType, NullLock, RDE_HASH, LFU> cm(cacheSize*2);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        izenelib::util::UString ystr;
        string str;
        ValueType val = 0;
        while (inf>>str) {
            sum++;
            val++;
            ystr = izenelib::util::UString(str, izenelib::util::UString::CP949);
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<str<<"->"<<val<<endl;
                cout<< "izeneCache (UString->int) numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH(UString->int) with "<<"CacheSize="<<cacheSize*2<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    cout<<"\n=================LRLFU==================="<<endl;
    trace = 0;
    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, RDE_HASH, LRLFU> cm(cacheSize*20);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH with "<<"CacheSize="<<cacheSize*2<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    {
        izenelib::cache::IzeneCache<KeyType, ValueType, NullLock, LINEAR_HASH, LRLFU> cm(cacheSize*20);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        string ystr;
        ValueType val = 0;
        while (inf>>ystr) {
            sum++;
            val++;
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<ystr<<"->"<<val<<endl;
                cout<< "MCache numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"LINEAR_HASH with "<<"CacheSize="<<cacheSize<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }

    trace = 0;
    {
        izenelib::cache::IzeneCache<izenelib::util::UString, ValueType, NullLock, RDE_HASH, LRLFU> cm(cacheSize*20);

        int sum =0;
        int hit =0;
        clock_t t1 = clock();
        ifstream inf(inputFile1.c_str());
        izenelib::util::UString ystr;
        string str;
        ValueType val = 0;
        while (inf>>str) {
            sum++;
            val++;
            ystr = izenelib::util::UString(str, izenelib::util::UString::CP949);
            if (cm.getValueWithInsert(ystr, val))
                hit++;
            else {
                //	outf<<ystr<<endl;
            }
            if (trace) {
                cout<< "get: key="<<str<<"->"<<val<<endl;
                cout<< "izeneCache (UString->int) numItem = "<<cm.numItems()<<endl;
                cm.displayHash();
                cout<<endl;
            }
        }

        cout<<"RDE_HASH(UString->int) with "<<"CacheSize="<<cacheSize*2<<endl;
        cout<<"Hit ratio: "<<hit<<" / "<<sum<<endl;
        cout<<"eclipse:"<< double(clock()- t1)/CLOCKS_PER_SEC<<endl;
        unsigned long vm = 0, rss = 0;

        ProcMemInfo::getProcMemInfo(vm, rss);

        //cout<<"memory usage: "<<cm.getMemSizeOfValue()<<"bytes"<<endl;
        cout << "vm: " << vm << "bytes rss: " << rss << "bytes" << endl;

    }
}

BOOST_AUTO_TEST_SUITE_END()

#endif /*T_IZENECACHE_CPP_*/
