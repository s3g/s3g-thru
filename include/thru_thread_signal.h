#pragma once
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace s3g_daisy {
// Persistent kernel wake tokens. The audio callback never waits for a worker.
// State is published separately through the owner's acquire/release atomics.
class ThruThreadSignal {
    int read_=-1,write_=-1;
public:
    ThruThreadSignal()=default;
    ThruThreadSignal(const ThruThreadSignal&)=delete;
    ThruThreadSignal& operator=(const ThruThreadSignal&)=delete;
    ~ThruThreadSignal(){if(read_>=0)close(read_);if(write_>=0)close(write_);}
    bool prepare(){
        if(read_>=0)return true;
        int descriptors[2];if(pipe(descriptors)!=0)return false;
        for(int fd:descriptors){
            const int flags=fcntl(fd,F_GETFL,0);
            if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0||fcntl(fd,F_SETFD,FD_CLOEXEC)<0){
                close(descriptors[0]);close(descriptors[1]);return false;
            }
        }
        read_=descriptors[0];write_=descriptors[1];return true;
    }
    void notify() noexcept {
        if(write_<0)return;
        const unsigned char token=1;
        for(int attempt=0;attempt<2;++attempt){
            if(write(write_,&token,1)==1)return;
            if(errno==EAGAIN)return; // A queued token already wakes the reader.
            if(errno!=EINTR)break;
        }
    }
    void wait() noexcept {
        pollfd descriptor{read_,POLLIN,0};
        // Timeout is a recovery path for an interrupted signal, not the normal
        // refill cadence. Notification wakes this wait immediately.
        (void)poll(&descriptor,1,2);
        unsigned char tokens[4096];
        (void)read(read_,tokens,sizeof(tokens));
        // Every notification leaves a pipe token, or finds an already full
        // readable pipe. No user-space pending flag can suppress a new wake
        // around a drain/idle/retire transition. Recheck owner state on return.
    }
};
}
