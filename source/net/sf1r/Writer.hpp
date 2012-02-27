/* 
 * File:   Writer.hpp
 * Author: Paolo D'Apice
 *
 * Created on January 10, 2012, 11:10 AM
 */

#ifndef WRITER_HPP
#define	WRITER_HPP

#include <boost/noncopyable.hpp>
#include <string>


NS_IZENELIB_SF1R_BEGIN


/**
 * Interface for request body writers.
 */
class Writer : private boost::noncopyable {
public:
    
    /// Destructor must not throw any exception.
    virtual ~Writer() throw() {};
    
    /**
     * Adds SF1 header into the body of the request.
     * @param controller
     * @param action
     * @param tokens
     */
    virtual void setHeader(const std::string& controller, 
                           const std::string& action,
                           const std::string& tokens,
                           std::string& request) const = 0;
    
    /**
     * Checks if the data is valid.
     * @param data the data to be checked.
     * @return true if data is valid, false otherwise.
     */
    virtual bool checkData(const std::string& data) const = 0;
    
};


NS_IZENELIB_SF1R_END

#endif	/* WRITER_HPP */