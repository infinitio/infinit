#pragma once
#ifndef ALGORITHM_WI0OXN2V
#define ALGORITHM_WI0OXN2V

# include <algorithm>
# include <boost/iterator/zip_iterator.hpp>
# include <boost/range.hpp>

namespace elle
{

using boost::tie;
using boost::get;

template <typename... T>
auto zip(T const &... containers) -> boost::iterator_range<boost::zip_iterator<decltype(boost::make_tuple(std::begin(containers)...))>>
{
    auto zip_begin = boost::make_zip_iterator(boost::make_tuple(std::begin(containers)...));
    auto zip_end = boost::make_zip_iterator(boost::make_tuple(std::end(containers)...));
    return boost::make_iterator_range(zip_begin, zip_end);
} 

}

#endif /* end of include guard: ALGORITHM_WI0OXN2V */
