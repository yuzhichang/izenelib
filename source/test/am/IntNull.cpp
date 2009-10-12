#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>
#include <boost/shared_ptr.hpp>

#include <am/tc/Hash.h>
#include <am/tc/BTree.h>
#include <am/sdb_hash/sdb_hash.h>
#include <am/sdb_btree/sdb_btree.h>
#include <am/sdb_storage/sdb_storage.h>
// #include <am/btree/BTreeFile.h>
// #include <am/skip_list_file/SkipListFile.h>
#include <am/cccr_hash/cccr_hash.h>
// #include <am/dynamic_perfect_hash/dynamic_perfect_hash.hpp>
// #include <am/linear_hash_table/linearHashTable.hpp>
// #include <am/map/map.hpp>
// #include <am/skip_list/skip_list.hpp>

#include <cstdio>

#include "TestDir.h"
#define DIR_PREFIX TEST_TMP_DIR "/am_IntNull_"

using namespace izenelib::am;

typedef boost::mpl::list<
    tc::Hash<int, NullType>
    , tc::BTree<int, NullType>
    , tc::BTree<int, NullType, tc::BTreeLessCmp<int> >
    , sdb_hash<int, NullType>
    , sdb_btree<int, NullType>
    //, BTreeFile<int, NullType>
    //, cccr_hash<int, NullType>
    //, sdb_storage<int, NullType>
    //, SkipListFile<int, NullType>
    //, DynamicPerfectHash<int, NullType>
    //, LinearHashTable<int, NullType>
    //, Map<int, NullType>
    //, SkipList<NullType, int>
> test_types;

namespace { // {anonymous}

template<typename T>
boost::shared_ptr<T> create(const std::string& file)
{
    const std::string path = DIR_PREFIX + file;
    std::remove(path.c_str());
    boost::shared_ptr<T> result(new T(path));
    BOOST_REQUIRE(result);
    BOOST_REQUIRE(result->open());

    return result;
}
template<>
boost::shared_ptr<cccr_hash<int, std::string> > create(const std::string&)
{
    boost::shared_ptr<cccr_hash<int, std::string> >
        result(new cccr_hash<int, std::string>());

    BOOST_REQUIRE(result);

    return result;
}

struct NullFixture
{
    NullFixture()
    : null()
    {}

    NullType null;
};
} // namespace {anonymous}

BOOST_FIXTURE_TEST_SUITE(IntString_test, NullFixture)

BOOST_AUTO_TEST_CASE_TEMPLATE(NewIsEmpty_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("NewIsEmpty_test");

    BOOST_CHECK(h->num_items() == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(InsertGet_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("InsertGet_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->get(1, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(GetFromEmtpy_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("GetFromEmtpy_test");

    BOOST_CHECK(!(h->get(1, null)));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(GetNotExisted_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("GetNotExisted_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(!h->get(2, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(InsertExisted_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("InsertExisted_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(!h->insert(1, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(InsertAndSize_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("InsertAndSize_test");

    {
        BOOST_CHECK(h->insert(1, null));
        BOOST_CHECK(h->num_items() == 1);
    }

    {
        BOOST_CHECK(h->insert(2, null));
        BOOST_CHECK(h->num_items() == 2);
    }

    {
        BOOST_CHECK(h->insert(3, null));
        BOOST_CHECK(h->num_items() == 3);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(UpdateAsInsert_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("UpdateAsInsert_test");

    BOOST_CHECK(h->update(1, null));
    BOOST_CHECK(h->num_items() == 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(Update_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("Update_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->update(1, null));
    BOOST_CHECK(h->num_items() == 1);
    BOOST_CHECK(h->get(1, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(Del_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("Del_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->num_items() == 1);
    BOOST_CHECK(h->del(1));
    BOOST_CHECK(h->num_items() == 0);
    BOOST_CHECK(!h->get(1, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(DelNotExisted_test, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("DelNotExisted_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->num_items() == 1);
    h->del(2);
    BOOST_CHECK(h->num_items() == 1);
    BOOST_CHECK(h->get(1, null));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(Update1In2, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("Update1In2_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->insert(2, null));
    BOOST_CHECK(h->update(1, null));

    BOOST_CHECK(h->num_items() == 2);
    h->get(1, null);
    h->get(2, null);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(Del1In2, T, test_types)
{
    boost::shared_ptr<T> h = create<T>("Del1In2_test");

    BOOST_CHECK(h->insert(1, null));
    BOOST_CHECK(h->insert(2, null));

    h->del(1);

    BOOST_CHECK(h->num_items() == 1);
    BOOST_CHECK(!h->get(1, null));
    BOOST_CHECK(h->get(2, null));
}

BOOST_AUTO_TEST_SUITE_END()