#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include "global.h"
#include "jobs.h"
#define DEBUG
char *envPath[10], cmdBuff[40];  //外部命令的存放路径及读取外部命令的缓冲空间
History history;                 //历史命令
Job *head = NULL;                //作业头指针
pid_t fgPid;                     //当前前台作业的进程号
static pid_t pgid;

/*******************************************************
                  工具以及辅助方法
********************************************************/
/*判断命令是否存在*/
int exists(char *cmdFile){
    int i = 0;
    if((cmdFile[0] == '/' || cmdFile[0] == '.') && access(cmdFile, F_OK) == 0){ //命令在当前目录
        strcpy(cmdBuff, cmdFile);
        return 1;
    }else{  //查找ysh.conf文件中指定的目录，确定命令是否存在
        while(envPath[i] != NULL){ //查找路径已在初始化时设置在envPath[i]中
            strcpy(cmdBuff, envPath[i]);
            strcat(cmdBuff, cmdFile);
            
            if(access(cmdBuff, F_OK) == 0){ //命令文件被找到
                return 1;
            }
            
            i++;
        }
    }
    
    return 0; 
}

/*将字符串转换为整型的Pid*/
int str2Pid(char *str, int start, int end){
    int i, j;
    char chs[20];
    
    for(i = start, j= 0; i < end; i++, j++){
        if(str[i] < '0' || str[i] > '9'){
            return -1;
        }else{
            chs[j] = str[i];
        }
    }
    chs[j] = '\0';
    
    return atoi(chs);
}

/*调整部分外部命令的格式*/
void justArgs(char *str){
    int i, j, len;
    len = strlen(str);
    
    for(i = 0, j = -1; i < len; i++){
        if(str[i] == '/'){
            j = i;
        }
    }

    if(j != -1){ //找到符号'/'
        for(i = 0, j++; j < len; i++, j++){
            str[i] = str[j];
        }
        str[i] = '\0';
    }
}


/*释放环境变量空间*/
void release(){
    int i;
    for(i = 0; strlen(envPath[i]) > 0; i++){
        free(envPath[i]);
    }
}

/*******************************************************
                  信号以及jobs相关
********************************************************/
//函数用于接受到子进程 结束、停止、继续 所发给父进程的信号
//但是这个信号实际上不一定能够接收得到
void sg_chld(int sig, siginfo_t *sip, void* noused){
	pid_t pid;
	pid_t p = 0;
	sigset_t sset;
	Job *now = NULL;
	now = head;

	fflush(stderr);
	if(sig != SIGCHLD)
		return;
	pid = sip->si_pid;
	sigfillset(&sset);
	sigdelset(&sset,SIGCHLD);
	switch(sip->si_code){
		case CLD_EXITED:
		case CLD_KILLED:
		case CLD_DUMPED:		//子进程结束

			while((p = waitpid(-1,NULL,WNOHANG))>0){	//回收僵尸进程，并从作业链表移除
				pid_t t = rmJob(p);						
				if(t<0 && !(fgPid+t)){					//如果移除导致撤销了前台作业，shell调回前台
					fgPid = 0;
					tcsetpgrp(STDIN_FILENO,getpid());
				}
			}
			return;
		case CLD_CONTINUED:					//子进程继续运行
			while(fgPid){					//fgPid如果有值，则是在fg_exe中赋值，此时阻塞shell进程
				sigsuspend(&sset);			
			}
			return;
		case CLD_STOPPED:
			if(getpgid(pid) == fgPid){		//停止的子进程如果是前台组，将shell调到前台
				fgPid = 0;
				tcsetpgrp(STDIN_FILENO,getpid());	
			}
			break;
		default:
			fprintf(stderr,"What condition!!!\n");
			return;
	}
    
    if(fgPid == 0){ //前台没有作业则直接返回
        return;
    }
}

/*fg命令*/
void fg_exec(int jid){    
    Job *now = NULL; 
	sigset_t sset;
	sigfillset(&sset);
	sigdelset(&sset,SIGCHLD);

	now = findJobId(jid);
	if(!now)
        printf("作业号为 %d 的作业不存在！\n",jid);
    
	fgPid = getJobpgid(now);
	tcsetpgrp(STDIN_FILENO,fgPid);
	killpg(fgPid,SIGCONT);
	while(fgPid)			//阻塞
		sigsuspend(&sset);
	fprintf(stderr,"out fg\n");
}

/*bg命令*/
void bg_exec(int pid){
	fgPid = 0;

    killpg(getpgid(pid), SIGCONT); //向对象作业发送SIGCONT信号，使其运行
}

/*type*/
void type_exec(char* s[]){
    int i,j;
    int flag = 0;
    int isBuiltin=0;
    //type have args -t -at -p -P
    if(strcmp(s[1],"-t")==0 || strcmp(s[1],"-at")==0||
        strcmp(s[1],"-p")==0||strcmp(s[1],"-P")==0){
        flag = 1;
    }
    for(i=1+flag;s[i]!=NULL;i++){
        isBuiltin = 0;
        //check is or not builtin order
        for(j=0;builtin[j]!=NULL;j++){
            if(strcmp(s[i],builtin[j])==0){
                isBuiltin=1;
                break;
            }
        }
        //print
        if(isBuiltin==1){
            printf("%s is built in the shell\n",s[i] );
        }
        else if(exists(s[i])){
             printf("%s is an outer order\n",s[i] );
        }
        else{
            printf("type: can not find %s \n",s[i] );   
        }
    }

}

/*echo*/
void echo_exec(char* s[]){
    int i;
    for(i=1;s[i]!=NULL;i++){
        printf("%s ",s[i]);
    }
    printf("\n");
}


/*******************************************************
                    命令历史记录
********************************************************/
void addHistory(char *cmd){
    if(history.end == -1){ //第一次使用history命令
        history.end = 0;
        strcpy(history.cmds[history.end], cmd);
        return;
	}
    
    history.end = (history.end + 1)%HISTORY_LEN; //end前移一位
    strcpy(history.cmds[history.end], cmd); //将命令拷贝到end指向的数组中
    
    if(history.end == history.start){ //end和start指向同一位置
        history.start = (history.start + 1)%HISTORY_LEN; //start前移一位
    }
}

/*******************************************************
                     初始化环境
********************************************************/
/*通过路径文件获取环境路径*/
void getEnvPath(int len, char *buf){
    int i, j,  pathIndex = 0, temp;
    char path[40];
    
    for(i = 0, j = 0; i < len; i++){
        if(buf[i] == ':'){ //将以冒号(:)分隔的查找路径分别设置到envPath[]中
            if(path[j-1] != '/'){
                path[j++] = '/';
            }
            path[j] = '\0';
            j = 0;
            
            temp = strlen(path);
            envPath[pathIndex] = (char*)malloc(sizeof(char) * (temp + 1));
            strcpy(envPath[pathIndex], path);
            
            pathIndex++;
        }else{
            path[j++] = buf[i];
        }
    }
    
    envPath[pathIndex] = NULL;
}

/*初始化操作*/
void init(){
    int fd, len;
    char c, buf[80];

	//打开查找路径文件ysh.conf
    if((fd = open("ysh.conf", O_RDONLY, 660)) == -1){
        perror("init environment failed\n");
        exit(1);
    }
    
	//初始化history链表
    history.end = -1;
    history.start = 0;
    
    len = 0;
	//将路径文件内容依次读入到buf[]中
    while(read(fd, &c, 1) != 0){ 
        buf[len++] = c;
    }
    buf[len] = '\0';

    //将环境路径存入envPath[]
    getEnvPath(len, buf); 
    
    //注册信号
    struct sigaction action;
    action.sa_sigaction = sg_chld;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
	signal(SIGTSTP,SIG_IGN);
	signal(SIGINT,SIG_IGN);
}

/*******************************************************
                      命令解析
********************************************************/
SimpleCmd* handleSimpleCmdStr(int begin, int end){
    int i, j, k;
    int fileFinished; //记录命令是否解析完毕
    char buff[10][40], inputFile[30], outputFile[30], *temp = NULL;
    SimpleCmd *cmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
    
	//默认为非后台命令，输入输出重定向为null
    cmd->isBack = 0;
    cmd->input = cmd->output = NULL;

	cmd->next = NULL;

	cmd->pipeIn = 0;
	cmd->pipeOut = 0;
    //初始化相应变量
    for(i = begin; i<10; i++){
        buff[i][0] = '\0';
    }
    inputFile[0] = '\0';
    outputFile[0] = '\0';
    
    i = begin;
	//跳过空格等无用信息
    while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
        i++;
    }
    
    k = 0;
    j = 0;
    fileFinished = 0;
    temp = buff[k]; //以下通过temp指针的移动实现对buff[i]的顺次赋值过程
    while(i < end){
		/*根据命令字符的不同情况进行不同的处理*/
        switch(inputBuff[i]){ 
            case ' ':
            case '\t': //命令名及参数的结束标志
                temp[j] = '\0';
                j = 0;
                if(!fileFinished){
                    k++;
                    temp = buff[k];
                }
                break;

            case '<': //输入重定向标志
                if(j != 0){
		    //此判断为防止命令直接挨着<符号导致判断为同一个参数，如果ls<sth
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = inputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '>': //输出重定向标志
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                temp = outputFile;
                fileFinished = 1;
                i++;
                break;
                
            case '&': //后台运行标志
                if(j != 0){
                    temp[j] = '\0';
                    j = 0;
                    if(!fileFinished){
                        k++;
                        temp = buff[k];
                    }
                }
                cmd->isBack = 1;
                fileFinished = 1;
                i++;
                break;
                
            default: //默认则读入到temp指定的空间
                temp[j++] = inputBuff[i++];
                continue;
		}
        
		//跳过空格等无用信息
        while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
            i++;
        }
	}
    
    if(inputBuff[end-1] != ' ' && inputBuff[end-1] != '\t' && inputBuff[end-1] != '&'){
        temp[j] = '\0';
        if(!fileFinished){
            k++;
        }
    }
    
	//依次为命令名及其各个参数赋值
    cmd->args = (char**)malloc(sizeof(char*) * (k + 1));
    cmd->args[k] = NULL;
    for(i = 0; i<k; i++){
        j = strlen(buff[i]);
        cmd->args[i] = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->args[i], buff[i]);
    }
    
	//如果有输入重定向文件，则为命令的输入重定向变量赋值
    if(strlen(inputFile) != 0){
        j = strlen(inputFile);
        cmd->input = (char*)malloc(sizeof(char) * (j + 1));
        strcpy(cmd->input, inputFile);
    }

    //如果有输出重定向文件，则为命令的输出重定向变量赋值
    if(strlen(outputFile) != 0){
        j = strlen(outputFile);
        cmd->output = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->output, outputFile);
    }
    #ifdef DEBUG
    printf("****\n");
    printf("isBack: %d\n",cmd->isBack);
    	for(i = 0; cmd->args[i] != NULL; i++){
    		printf("args[%d]: %s\n",i,cmd->args[i]);
	}
    printf("input: %s\n",cmd->input);
    printf("output: %s\n",cmd->output);
    printf("****\n");
    #endif
    return cmd;
}

/*******************************************************
                      命令执行
********************************************************/
/*执行外部命令*/
void execOuterCmd(SimpleCmd *cmd){
    pid_t pid;
    int pipeIn, pipeOut;
	static int tmpp;
	int p[2] = {0,0};
    
	if(cmd->next){
		if(pipe(p)){
			fprintf(stderr,"Pipe failed\n");
			goto EXIT;
		}
		cmd->pipeOut = p[1];
		cmd->next->pipeIn = p[0];
	}
    if(exists(cmd->args[0])){ //命令存在
			sigset_t set;
			sigfillset(&set);

        if((pid = vfork()) < 0){
            perror("fork failed");
			goto EXIT;
        }
        
        if(pid == 0){ //子进程
			if(!pgid)
				pgid = getpid();
			setpgid(getpid(),pgid);
			signal(SIGCHLD,SIG_DFL);
			signal(SIGINT,SIG_DFL);
			signal(SIGTSTP,SIG_DFL);
			signal(SIGUSR1,SIG_DFL);
			signal(SIGUSR2,SIG_DFL);
			addJob(getpid());
			if(cmd->pipeIn){
				dup2(cmd->pipeIn,0);
				close(cmd->pipeIn);
			}
			if(cmd->pipeOut){
				dup2(cmd->pipeOut,1);
				close(p[1]);
				close(p[0]);
			}
            if(cmd->input != NULL){ //存在输入重定向
                if((pipeIn = open(cmd->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                    printf("不能打开文件 %s！\n", cmd->input);
                    return;
                }
                if(dup2(pipeIn, 0) == -1){
                    printf("重定向标准输入错误！\n");
                    return;
                }
            }
            
            if(cmd->output != NULL){ //存在输出重定向
                if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                    printf("不能打开文件 %s！\n", cmd->output);
                    return ;
                }
                if(dup2(pipeOut, 1) == -1){
                    printf("重定向标准输出错误！\n");
                    return;
                }
            }
            
            if(cmd->isBack){ 
                printf("\n[%d]\t%s\t\t%s\n", getpid(), RUNNING, inputBuff);
            }
            
            justArgs(cmd->args[0]);
            if(execv(cmdBuff, cmd->args) < 0){ //执行命令
                printf("execv failed!\n");
                return;
            }
        }
		else{ //父进程
		void execSimpleCmd(SimpleCmd *cmd);//declare
			if(!fgPid)
				fgPid = pid;
			if(tmpp){
				close(tmpp);
				tmpp = 0;
			}
			if(cmd->next){
			tmpp = p[0];
				close(p[1]);
				execSimpleCmd(cmd->next);
				goto EXIT;
			}
            if(cmd ->isBack){ //后台命令             
                fgPid = 0; //pid置0，为下一命令做准备
				pgid = 0;
            }else{ //非后台命令
				sigset_t pbm;
				sigfillset(&pbm);
				sigdelset(&pbm,SIGCHLD);

				tcsetpgrp(STDIN_FILENO,pgid);//不用多次使用，STDIN和STDOUT都已被改变
				pgid=0;
				while(fgPid){
					sigsuspend(&pbm);
				}
            }
		}
    }else{ //命令不存在
		if(cmd->pipeIn)
			close(cmd->pipeIn);
		if(cmd->pipeOut)
			close(cmd->pipeOut);
        printf("找不到命令 15%s\n", inputBuff);
    }
EXIT:
	pgid = 0;
}

/*执行命令*/
void execSimpleCmd(SimpleCmd *cmd){
    int i, pid;
    char *temp;
    //Job *now = NULL;
    
    if(strcmp(cmd->args[0], "exit") == 0) { //exit命令
        exit(0);
    } else if (strcmp(cmd->args[0], "history") == 0) { //history命令
        if(history.end == -1){
            printf("尚未执行任何命令\n");
            return;
        }
        i = history.start;
        do {
            printf("%s\n", history.cmds[i]);
            i = (i + 1)%HISTORY_LEN;
        } while(i != (history.end + 1)%HISTORY_LEN);
    } else if (strcmp(cmd->args[0], "jobs") == 0) { //jobs命令
            printf("index\tpid\tstate\t\tcommand\n");
			displayJobs();
    } else if (strcmp(cmd->args[0], "cd") == 0) { //cd命令
        temp = cmd->args[1];
        if(temp != NULL){
            if(chdir(temp) < 0){
                printf("cd; %s 错误的文件名或文件夹名！\n", temp);
            }
        }
    } else if (strcmp(cmd->args[0], "fg") == 0) { //fg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            if(pid != -1){
                fg_exec(pid);
            }
        }else{
            printf("fg; 参数不合法，正确格式为：fg %%<int>\n");
        }
    } else if (strcmp(cmd->args[0], "bg") == 0) { //bg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            
            if(pid != -1){
                bg_exec(pid);
            }
        }
		else{
            printf("bg; 参数不合法，正确格式为：bg %%<int>\n");
        }
    }else if(strcmp(cmd->args[0],"type")==0){
            type_exec(cmd->args);
    } 
    else if(strcmp(cmd->args[0],"echo")==0){
        echo_exec(cmd->args);
    }
    else{ //外部命令
        execOuterCmd(cmd);
    }
    
    //释放结构体空间
    for(i = 0; cmd->args[i] != NULL; i++){
        free(cmd->args[i]);
    }
    free(cmd->input);
    free(cmd->output);
	free(cmd->args[i]);
	free(cmd->args);
	free(cmd);
}

/*******************************************************
                     命令执行接口
********************************************************/
void execute(){
    SimpleCmd *cmd = NULL;
	SimpleCmd *p = NULL;
	int i,l,j;
	l=strlen(inputBuff);
	for(i=0,j=0;i<l;++i){
		if(inputBuff[i]!='|')
			continue;
		if(!cmd)
			cmd = handleSimpleCmdStr(j,i);
		else{
			for(p=cmd;p->next!=NULL;p=p->next);
			p->next = handleSimpleCmdStr(j,i);
		}
		j = i+1;
	}
	if(!cmd)
			cmd = handleSimpleCmdStr(j,i);
		else{
			for(p=cmd;p->next!=NULL;p=p->next);
			p->next = handleSimpleCmdStr(j,i);
		}
    execSimpleCmd(cmd);
}
