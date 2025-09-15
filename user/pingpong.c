#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void main(int argc,char* argv[]){
    int p[2];
    pipe(p);
    if(fork()==0){
        char buf[100];
        close(0);
        dup(p[0]);
        //close(p[1]);
       // close(p[0]);
        int n = read(0, buf, 4);
        buf[n] = '\0';   // 添加 '\0'
        printf("%d: received %s\n", getpid(), buf);
        write(p[1], "pong", 4);
       // close(p[1]);
        exit(0);
    }else{
        //close(p[0]);
        write(p[1], "ping", 4);
        wait(0);
        close(0);
        dup(p[0]);
       // close(p[0]);
        //close(p[1]);
        char buf[100];
        int n = read(0, buf, 4);
        buf[n] = '\0';   // 添加 '\0'
        printf("%d: received %s\n", getpid(), buf);
        exit(0);
    }

}