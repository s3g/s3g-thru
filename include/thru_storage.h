#pragma once
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
namespace s3g_thru_storage {
enum class Operation { fileSync, directorySync, rename, link };
#ifdef S3G_THRU_STORAGE_TESTING
inline bool (*testHook)(Operation,const char*,const char*)=nullptr;
inline bool allow(Operation op,const char* a,const char* b=nullptr) {
    if(testHook&&!testHook(op,a,b)){errno=EIO;return false;}return true;
}
#else
inline bool allow(Operation,const char*,const char* =nullptr){return true;}
#endif
inline bool syncFd(int fd) {
    int result;do {result=::fsync(fd);}while(result<0&&errno==EINTR);
    return result==0;
}
inline bool closeFile(std::FILE* file,const char* path,bool valid=true) {
    if(!file)return false;
    const bool flushed=std::fflush(file)==0;
    const bool synced=valid&&flushed&&allow(Operation::fileSync,path)&&syncFd(fileno(file));
    const bool closed=std::fclose(file)==0;
    return synced&&closed;
}
inline bool syncParent(const char* path) {
    if(!path||!path[0])return false;
    std::array<char,4096> directory{};
    if(std::strlen(path)>=directory.size())return false;
    std::strcpy(directory.data(),path);
    auto* slash=std::strrchr(directory.data(),'/');
    if(!slash)std::strcpy(directory.data(),".");
    else if(slash==directory.data())slash[1]=0;
    else *slash=0;
    if(!allow(Operation::directorySync,directory.data()))return false;
    const int fd=::open(directory.data(),O_RDONLY|O_DIRECTORY);
    if(fd<0)return false;
    const bool synced=syncFd(fd);const bool closed=::close(fd)==0;
    return synced&&closed;
}
inline bool rename(const char* from,const char* to) {
    return allow(Operation::rename,from,to)&&std::rename(from,to)==0;
}
inline bool link(const char* from,const char* to) {
    return allow(Operation::link,from,to)&&::link(from,to)==0;
}
} // namespace s3g_thru_storage
