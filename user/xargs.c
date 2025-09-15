#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h" // for MAXARG

int main(int argc, char *argv[]) {
    char *new_argv[MAXARG];
    char line_buf[512];
    int base_argc = 0;
    int i;

    // 1. 复制基础命令参数 (跳过 "xargs" 本身)
    for (i = 1; i < argc; i++) {
        new_argv[base_argc++] = argv[i];
    }

    // 2. 循环读取标准输入的一行
    while (1) {
        char *p = line_buf;
        int read_result;
        // 读取一整行到 line_buf
        while ((read_result = read(0, p, 1)) > 0) {
            if (*p == '\n') {
                break; // 读到换行符，一行结束
            }
            p++;
            // 安全检查: 防止行太长导致缓冲区溢出
            if (p >= line_buf + sizeof(line_buf)) {
                fprintf(2, "xargs: line too long\n");
                exit(1);
            }
        }
        
        // 【关键判断逻辑】在这里判断是否应该退出主循环
        // 如果 read_result <= 0 (意味着 EOF 或错误) 
        // 并且 p == line_buf (意味着当前行没有读到任何字符)
        // 那么就说明所有输入都已处理完毕
        if (read_result <= 0 && p == line_buf) {
            break; // 退出外层的 while(1) 循环
        }
        *p='\0';
        int current_argc = base_argc;
        new_argv[current_argc++] = line_buf;
        new_argv[current_argc] = 0; // 放置 NULL 结束符

        // 4. fork 和 exec
        if (fork() == 0) {
            // 子进程
            exec(new_argv[0], new_argv);
            fprintf(2, "exec %s failed\n", new_argv[0]);
            exit(1);
        } else {
            // 父进程
            wait(0);
        }
    }

    exit(0);
}