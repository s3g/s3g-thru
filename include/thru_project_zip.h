#pragma once
#include "thru_zip.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
namespace s3g_thru_project {
struct ZipBudget {std::size_t used=0;static constexpr std::size_t limit=8u*1024u*1024u;};
struct alignas(std::max_align_t) ZipAllocation {std::size_t bytes;};
inline void* zipAllocate(void* opaque,std::size_t count,std::size_t size) {
    auto& budget=*static_cast<ZipBudget*>(opaque);
    if(size&&count>std::numeric_limits<std::size_t>::max()/size)return nullptr;
    const auto bytes=count*size;if(bytes>ZipBudget::limit-budget.used)return nullptr;
    auto* p=static_cast<ZipAllocation*>(std::malloc(sizeof(ZipAllocation)+bytes));if(!p)return nullptr;
    p->bytes=bytes;budget.used+=bytes;return p+1;
}
inline void zipFree(void* opaque,void* pointer) {
    if(!pointer)return;
    auto& budget=*static_cast<ZipBudget*>(opaque);
    auto* header=static_cast<ZipAllocation*>(pointer)-1;budget.used-=header->bytes;std::free(header);
}
inline void* zipReallocate(void* opaque,void* pointer,std::size_t count,std::size_t size) {
    if(!pointer)return zipAllocate(opaque,count,size);
    if(!count||!size){zipFree(opaque,pointer);return nullptr;}
    auto& budget=*static_cast<ZipBudget*>(opaque);auto* old=static_cast<ZipAllocation*>(pointer)-1;
    if(count>std::numeric_limits<std::size_t>::max()/size)return nullptr;
    const auto bytes=count*size;if(bytes>ZipBudget::limit-(budget.used-old->bytes))return nullptr;
    const auto previous=old->bytes;auto* next=static_cast<ZipAllocation*>(std::realloc(old,sizeof(ZipAllocation)+bytes));if(!next)return nullptr;
    next->bytes=bytes;budget.used=budget.used-previous+bytes;return next+1;
}
inline void setupZip(mz_zip_archive& zip,ZipBudget& budget) {
    zip.m_pAlloc=zipAllocate;zip.m_pRealloc=zipReallocate;zip.m_pFree=zipFree;zip.m_pAlloc_opaque=&budget;
}
} // namespace s3g_thru_project
