#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void child(int *p){
    int prime=-1;
    int num=0;
    close(p[1]);
    int pp[2];
    pipe(pp);
    int cnt=0;
    int is_fork=0;
    while(read(p[0],&num,sizeof(int))>0){
        if(cnt==0){
            prime=num;
            printf("prime %d\n",prime);
        }
        if(num%prime!=0){
            if(is_fork==0){
                is_fork=1;
                int pid=fork();
                if(pid==0){
                    close(p[0]);
                    child(pp);
                }else{
                    close(pp[0]);
                    write(pp[1],&num,sizeof(int));
                }
            }else{
                write(pp[1],&num,sizeof(int));
            }
        }
        cnt++;
        
    }
    close(p[0]);
    close(pp[1]);
    wait(0);
    exit(0);
}

void main(int argc,char* argv[]){
    int p[2];
    pipe(p);
    int pid=fork();
    if(pid!=0){
        close(p[0]);
        for(int i=2;i<=35;i++){
            write(p[1],(void *)&i,sizeof(i));
        }
        close(p[1]);
        wait(0);
        exit(0);
    }else{
        child(p);
    }
    


}