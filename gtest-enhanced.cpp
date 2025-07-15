#include "gtest-enhanced.h"
#include "googletest/googletest/src/gtest-all.cc"
#include "googletest/googlemock/src/gmock-all.cc"


TestCout::~TestCout()
{
    testing::internal::ColoredPrintf(testing::internal::GTestColor::kGreen, "[          ] ");
    testing::internal::ColoredPrintf(testing::internal::GTestColor::kYellow, "%s", Source.str().c_str());
}
