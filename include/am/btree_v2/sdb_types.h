#ifndef SDB_TYPES_H_
#define SDB_TYPES_H_

#include <types.h>
#include <am/am.h>
#include <am/concept/DataType.h>

#include <am/util/DbObj.h>
#include <am/util/Wrapper.h>
#include <am/util/MemMap.h>
#include <am/util/SdbUtil.h>

#include <util/RefCount.h>

//#include <boost/memory.hpp>
#include <boost/static_assert.hpp>

//#include "BTreeFileException.h"

using namespace izenelib::am::util;

NS_IZENELIB_AM_BEGIN

const int BOOST_SERIAZLIZATION_HEAD_LENGTH = 40;

NS_IZENELIB_AM_END

#endif /*SDB_TYPES_H_*/
