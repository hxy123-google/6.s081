#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void main(int argc,char* argv[]){
    if(argc!=2){
        printf("wrong arguments\n");
        exit(0);     
    }else{
        int n=atoi(argv[1]);
        sleep(n);
        exit(0);
    }
}