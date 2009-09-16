#ifndef PROPERTY_ITEM_H
#define PROPERTY_ITEM_H

#include <boost/serialization/shared_ptr.hpp>

#include <deque>

struct PropertyItem
{
    PropertyItem():positions(new std::deque<unsigned int>){}
    boost::shared_ptr<std::deque<unsigned int> > positions;
    size_t tf;
    size_t doclen;
};

struct PropertyItem2
{
    PropertyItem2():positions(new std::deque<std::pair<unsigned int,unsigned int> >){}
    boost::shared_ptr<std::deque<std::pair<unsigned int,unsigned int> > > positions;
    size_t tf;
    size_t doclen;
};


#endif
