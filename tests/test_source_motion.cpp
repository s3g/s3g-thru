#define S3G_THRU_STORAGE_TESTING 1
#define S3G_THRU_HOST_TESTING 1
#include "../src/dsp/thru_module.cpp"
#include <cassert>
#include <filesystem>
static void renderSplit(ThruInstance& t,int n){std::array<int16_t,256> a{};for(int i=0;i<n;++i)t.render(a.data(),128);}
#include "test_source_motion.inc"
int main(){testSourceMotion();}
