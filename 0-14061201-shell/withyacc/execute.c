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
#include<pwd.h>
#include "global.h"
//#define DEBUG
int length;
int goon = 0, ingnore = 0;       //to set signal
char *envPath[10], cmdBuff[40];  //to save outer and inner cmd
History history;                 //history cmd
Job *head = NULL;                //Job's first array
pid_t fgPid;                     //current front process pid
pNode* headPipe=NULL;
void (*old_handler1)(int);
void (*old_handler2)(int);
void (*old_handler3)(int);
//SIGINT: ctrl+c
//SIGTSTP:ctrl+z
//SIGTTOU:bg process try writing
//SIGTTIN:bg process try reading
//SIGSTOP: stop executing
//SIGCONT: continue doing stopped process
//SIGUSR1:usr define
//SIGUSR2:usr define
/*********************************************************
				Pipe method
**********************************************************/
/*print*/
void ctrl_cz()
{
    struct passwd *pwd=getpwuid(getuid());
    int len=32;
    char hostname[len];
    gethostname(hostname,len);
    printf("\nyourname%s@%s:%s$ ", pwd->pw_name, hostname, get_current_dir_name()); //print    
    fflush(stdout);
}
pNode* addpNode(pNode* g, int read, int write, pid_t pid)
{
    pNode* tmp=(pNode*)malloc(sizeof(pNode));
    tmp->pid=pid;
    tmp->readPipe=read;
    tmp->writePipe=write;
    tmp->status=1;
    tmp->next=g;
    return tmp;
}
/*stop Pipe******************/
void exitPipe(int sig, siginfo_t *sip, void* hehe)
{
    pNode* p;
    pid_t pid=sip->si_pid;
    if (sip->si_status==SIGTSTP||sip->si_status==SIGCONT||sip->si_status==SIGSTOP||sip->si_status==SIGTTOU||sip->si_status==SIGTTIN) {
			return;
    }
    for (p=headPipe;p!=NULL;p=p->next)
    {
        if (p->pid==pid&&p->status)
        {
            p->status=0;
            close(p->readPipe);
            close(p->writePipe);
            break;
        }
    }
    ingnore++;
    //printf("%d: %d\n", ii, status);
}
void stopPipe()
{
    pNode* p;
    for (p=headPipe;p!=NULL;p=p->next){
        if (p->status){
            kill(p->pid,SIGTSTP);
        }
    }
}
/*restart pipe*******************************************/
void restartPipe()
{
    pNode* p;
    for (p=headPipe;p!=NULL;p=p->next){
        if (p->status){
            kill(p->pid,SIGCONT);
        }
    }
}
/*realize pipe**********************/
void executePipe(SimpleCmd *cmd)
{
    pid_t pid;
    int number, i;
    int fd[2], pipeIn, pipeOut, piRd=-1;
    char hehehe[1]="";
    headPipe=NULL;
    struct sigaction action;
    action.sa_sigaction = exitPipe;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
    signal(SIGTSTP, old_handler1);
    signal(SIGINT, old_handler2);
    signal(SIGTTOU, old_handler3);    
    signal(SIGCONT, restartPipe);

    SimpleCmd* p=cmd;
    while (p!=NULL){
        if (!exists(p->args[0])) {
            printf("can't find cmd %s\n", p->args[0]);
            exit(0);
        }
	if (p->next!=NULL&&(p->output!=NULL||p->next->input!=NULL)){
            printf("pipe repeat");
            putchar('\n');
            exit(0);
        }
        if (p->next!=NULL){//the pipe needs to be chongdingxiang 
            p->output=hehehe;
            p->next->input=hehehe;
        }
        p=p->next;
    }
    
    p=cmd;
    number=0;
    ingnore=0;

    while (p!=NULL)
    {
        if (p->next!=NULL)
        {
            if (pipe(fd)==-1)
            {
                printf("create pipe failed");
                putchar('\n');
                exit(0);
            }
        }
        else {
            fd[1]=-1;
        }
            pid=fork();
            number++;
            if (pid==0)
            break;
            else {
                headPipe=addpNode(headPipe,pid,piRd,fd[1]);
            }
            piRd = fd[0];
            p=p->next;
    }
    i=0;
    if (pid>0) 
    {
        for (i=0;i<number*2+3;i++) close(i);
        while (ingnore<number)
        {
        }
        exit(0);
    }
    if (p->next!=NULL) 
	close(fd[0]);
    if (p->input!=NULL)
    {
        if (strlen(p->input)>0)
        {
            if((pipeIn = open(p->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                printf("can not open the file %s \n", p->input);
                exit(0);
            }
        }
        else
        {
            pipeIn=piRd;           
        }
    }
	else 
	pipeIn=0;
    if (p->output!=NULL)
    {
        if (strlen(p->output)>0)
        {
            if((pipeOut = open(p->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                printf("can not open the file %s\n", p->output);
                exit(0);
            }
        }
        else
        {
            pipeOut=fd[1];          
        }
    }
	else 
	pipeOut=1;
    if(dup2(pipeIn, 0) == -1){
        printf("reDirection standard input is error\n");
        exit(0);
    }
    if(dup2(pipeOut, 1) == -1){
        printf("reDirection standard output is error\n");
        exit(0);
    }
    for (i=3;i<=3+number*2;++i) 
    {
        close(i);
    }
    if (pipeIn!=0) 
	close(pipeIn); 
    if (pipeOut!=1) 
	close(pipeOut); 
    if (!exists(p->args[0])) exit(0);
    justArgs(p->args[0]);            
    if(execvp(cmdBuff, p->args) < 0){ /**/
        printf("execv failed\n");
        exit(0);
    }       
}
/*******************************************************
                  method
********************************************************/

/*judge if cmd exists*/
int exists(char *cmdFile){
    int i = 0;
    if(access(cmdFile, F_OK) == 0&&(cmdFile[0] == '/' || cmdFile[0] == '.')){ //ÃüÁîÔÚµ±Ç°Ä¿ÂŒ
        strcpy(cmdBuff, cmdFile);
        return 1;
    }
	else{  //²éÕÒysh.confÎÄŒþÖÐÖž¶šµÄÄ¿ÂŒ£¬È·¶šÃüÁîÊÇ·ñŽæÔÚ
        while(envPath[i] != NULL){ //²éÕÒÂ·Ÿ¶ÒÑÔÚ³õÊŒ»¯Ê±ÉèÖÃÔÚenvPath[i]ÖÐ
            strcpy(cmdBuff, envPath[i]);
            strcat(cmdBuff, cmdFile);
            if(access(cmdBuff, F_OK) == 0){ //ÃüÁîÎÄŒþ±»ÕÒµœ
                return 1;
            }
            i++;
        }
    }
    return 0; 
}

/*make pid from string to int*/
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

/*adjust part of outer cmd pattern*/
void justArgs(char *str){
    int i, j, len;
    len = strlen(str);
    
    for(i = 0, j = -1; i < len; i++){
        if(str[i] == '/'){
            j = i;
        }
    }

    if(j != -1){ //ÕÒµœ·ûºÅ'/'
        for(i = 0, j++; j < len; i++, j++){
            str[i] = str[j];
        }
        str[i] = '\0';
    }
}

/*set goon*/
void setGoon(){
    goon = 1;
}

/*release space*/
void release(){
    int i;
    for(i = 0; strlen(envPath[i]) > 0; i++){
        free(envPath[i]);
    }
}

/*******************************************************
                  signal and jobs
********************************************************/
/*add new job*/
Job* addJob(pid_t pid){
    Job *now = NULL, *last = NULL, *job = (Job*)malloc(sizeof(Job));
    
	//initial new job
    job->pid = pid;
    strcpy(job->cmd, inputBuff);
    strcpy(job->state, RUNNING);
    job->next = NULL;
    
    if(head == NULL){ //
        head = job;
    }else{ //
		now = head;
		while(now != NULL && now->pid < pid){
			last = now;
			now = now->next;
		}
        last->next = job;
        job->next = now;
    }
    
    return job;
}

/*remove first job*/
void rmJob(int sig, siginfo_t *sip, void* noused){
    pid_t pid;
    Job *now = NULL, *last = NULL;
    if (sip->si_status==SIGTSTP&&fgPid==0||sip->si_status==SIGTTOU||sip->si_status==SIGTTIN){
		tmpsleep(sip->si_pid);
    }
    if (sip->si_status==SIGTSTP||sip->si_status==SIGSTOP){
		tmpsleep(sip->si_pid);
    }
    if (sip->si_status==SIGCONT||sip->si_status==SIGTSTP||sip->si_status==SIGSTOP||sip->si_status==SIGTTOU||sip->si_status==SIGTTIN){
		return;
    }
    pid = sip->si_pid;
    now = head;
    while(now != NULL && now->pid < pid){
	last = now;
	now = now->next;
    }
    
    if(now == NULL){ //if job is null ,then return
        return;
    }
    
	//remove job
    if(now == head){
        head = now->next;
    }
    else{
        last->next = now->next;
    }
    waitpid(pid,NULL,WUNTRACED);//when no son exit,father do not stop
    free(now);
}
/*set bg process sleep*/
void tmpsleep(int pid){
    Job *now = NULL;
    
    if(pid == 0){ 
        return;
    }
        
    now = head;
    while(now != NULL && now->pid != pid)
        now = now->next;
    
    if(now == NULL){ //if can't find fg process，then addJob(pid)
        now = addJob(pid);
    }
    
    //change fg process and situation,then print 
    strcpy(now->state, STOPPED); 
    printf("\n[%d]\t%s\t%s\n", now->pid, now->state, now->cmd);
}
/*fg  cmd*/
void fg_exec(int pid){    
    Job *now = NULL; 
	int i;
	//according to pid,to find job
    now = head;
    while(now != NULL && now->pid != pid)
	now = now->next;
    
    if(now == NULL){ //can not find job
        printf("pidÎª7%d µÄ×÷Òµ²»ŽæÔÚ£¡\n", pid);
        return;
    }

    //record fg pid and change it's states
    fgPid = now->pid;
    strcpy(now->state, RUNNING);
    
  //  signal(SIGTSTP, old_handler1); //set signal，get ready for Ctrl+Z
   // signal(SIGINT, old_handler2); //set signal，get ready for ctrl+C
 //   i = strlen(now->cmd) - 1;
 //   while(i >= 0 && now->cmd[i] != '&')
 //		i--;
 //   now->cmd[i] = '\0';
    printf("running %s\n", now->cmd);
    if (tcsetpgrp(0,fgPid)<0){//change terminal read ,write ,error
	printf("stdinfailed\n"); 
    }  
    if (tcsetpgrp(1,fgPid)<0){
        printf("stdoutfailed\n");
    }
    if (tcsetpgrp(2,fgPid)<0){
        printf("errorfailed\n");
    }
    kill(now->pid, SIGCONT); //sen continue message to job ,and make run
    sleep(1);
    waitpid(fgPid, NULL, WUNTRACED); //father wait for fg 

    if (tcsetpgrp(0,getpid())<0){//get back terminal
         printf("stdinfailed\n");    
    }
    if (tcsetpgrp(1,getpid())<0){
	 printf("stdoutfailed\n");
    }
    if (tcsetpgrp(2,getpid())<0){
         printf("errorfailed\n"); 
    } 
}

/*bgÃüÁî*/
void bg_exec(int pid){
    Job *now = NULL;
    
    //SIGCHLDÐÅºÅ²úÉú×ÔŽËº¯Êý
    //ingnore = 1;
    
	//žùŸÝpid²éÕÒ×÷Òµ
	now = head;
    while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ //ÎŽÕÒµœ×÷Òµ
        printf("pidÎª7%d µÄ×÷Òµ²»ŽæÔÚ£¡\n", pid);
        return;
    }
    
    strcpy(now->state, RUNNING); //ÐÞžÄ¶ÔÏó×÷ÒµµÄ×ŽÌ¬
   // setpgid(now->pid,pid);
   // setsid();
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
    kill(now->pid, SIGCONT); //Ïò¶ÔÏó×÷Òµ·¢ËÍSIGCONTÐÅºÅ£¬Ê¹ÆäÔËÐÐ
}

/*******************************************************
                    history cmd record
********************************************************/
void addHistory(char *cmd){
    if(history.end == -1){ //µÚÒ»ŽÎÊ¹ÓÃhistoryÃüÁî
        history.end = 0;
        strcpy(history.cmds[history.end], cmd);
        return;
	}
    
    history.end = (history.end + 1)%HISTORY_LEN; //endÇ°ÒÆÒ»Î»
    strcpy(history.cmds[history.end], cmd); //œ«ÃüÁî¿œ±ŽµœendÖžÏòµÄÊý×éÖÐ
    
    if(history.end == history.start){ //endºÍstartÖžÏòÍ¬Ò»Î»ÖÃ
        history.start = (history.start + 1)%HISTORY_LEN; //startÇ°ÒÆÒ»Î»
    }
}

/*******************************************************
                     initial environment
********************************************************/
/*Íš¹ýÂ·Ÿ¶ÎÄŒþ»ñÈ¡»·Ÿ³Â·Ÿ¶*/
void getEnvPath(int len, char *buf){
    int i, j, last = 0, pathIndex = 0, temp;
    char path[40];
    
    for(i = 0, j = 0; i < len; i++){
        if(buf[i] == ':'){ //œ«ÒÔÃ°ºÅ(:)·ÖžôµÄ²éÕÒÂ·Ÿ¶·Ö±ðÉèÖÃµœenvPath[]ÖÐ
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

/*initial*/
void init(){
    int fd, n, len;
    char c, buf[80];

	//open ysh.conf which find file path
    if((fd = open("ysh.conf", O_RDONLY, 660)) == -1){
        perror("init environment failed\n");
        exit(1);
    }
    
	//³õÊŒ»¯historyÁŽ±í
    history.end = -1;
    history.start = 0;
    
    len = 0;
	//œ«Â·Ÿ¶ÎÄŒþÄÚÈÝÒÀŽÎ¶ÁÈëµœbuf[]ÖÐ
    while(read(fd, &c, 1) != 0){ 
        buf[len++] = c;
    }
    buf[len] = '\0';

    //œ«»·Ÿ³Â·Ÿ¶ŽæÈëenvPath[]
    getEnvPath(len, buf); 
    
    //×¢²áÐÅºÅ
    struct sigaction action;
    action.sa_sigaction = rmJob;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
    old_handler1 = signal(SIGTSTP, ctrl_cz);
    old_handler2 = signal(SIGINT, ctrl_cz);
    old_handler3 = signal(SIGTTOU, SIG_IGN);//ignore the signal
}

/*******************************************************
                     cmd analysis
********************************************************/
SimpleCmd* handleSimpleCmdStr(int begin, int end){
    int i, j, k;
    int fileFinished; //record cmd if execute finished
    char c, buff[10][40], inputFile[30], outputFile[30], *temp = NULL;
    SimpleCmd *cmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
    int nextPipe=0;//mark next pipe position
	//
    cmd->isBack = 0;
    cmd->input = NULL;
    cmd->output = NULL;
    cmd->next=NULL;
    //默认为非后台命令，输入输出重定向为null
    for(i = begin; i<10; i++){
        buff[i][0] = '\0';
    }
    inputFile[0] = '\0';
    outputFile[0] = '\0';
    
    i = begin;
	//初始化相应变量
    while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
        i++;
    }
    
    k = 0;
    j = 0;
    fileFinished = 0;
    nextPipe=0;
    temp = buff[k]; //以下通过temp指针的移动实现对buff[i]的顺次赋值过程
    while(i < end){
		/*根据命令字符的不同情况进行不同的处理*/
        switch(inputBuff[i]){ 
            case '|'://"  " and "/t" and "|" is
            case ' ':
            case '\t': //ÃüÁîÃûŒ°²ÎÊýµÄœáÊø±êÖŸ
                temp[j] = '\0';
                j = 0;
		        if(i-1<0||inputBuff[i]!='|'||(inputBuff[i-1]!=' '&&inputBuff[i-1]!='\t'))
                if(!fileFinished){
                    k++;
                    temp = buff[k];
                }
                if('|'==inputBuff[i])
                {
                    nextPipe=i+1;
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
                if(nextPipe){
			break;
                }
		//跳过空格等无用信息
        while(i < end && (inputBuff[i] == ' ' || inputBuff[i] == '\t')){
            i++;
        }
	}
    
    if(inputBuff[end-1] != ' ' && inputBuff[end-1] != '\t' && inputBuff[end-1] != '&'&&!nextPipe){
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
    length=k;
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
   // #ifdef DEBUG
    //printf("****\n");
    //printf("isBack: %d\n",cmd->isBack);
    //	for(i = 0; cmd->args[i] != NULL; i++){
    //		printf("args[%d]: %s\n",i,cmd->args[i]);
	//}
    //printf("input: %s\n",cmd->input);
    //printf("output: %s\n",cmd->output);
    //printf("****\n");
    //#endif
    
    if(nextPipe!=0) 
    {
		
        cmd->next = handleSimpleCmdStr(nextPipe,end);
        cmd->isBack = 0;
        cmd->next->isBack = 0;
    }
    return cmd;
}

/*******************************************************
                     cmd execute
********************************************************/
/*ÖŽÐÐÍâ²¿ÃüÁî*/
void execOuterCmd(SimpleCmd *cmd){
    pid_t pid;
    int pipeIn, pipeOut;
    
    if(exists(cmd->args[0])){ //命令存在

        if((pid = fork()) < 0){
            perror("fork failed");
            return;
        }
        
        if(pid == 0){ //son process
			setpgid(0,0);
			if(cmd->next==NULL){
				if(cmd->input != NULL){ //存在输入重定向
					if((pipeIn = open(cmd->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
						printf("can not open the file %s\n", cmd->input);
						return;
					}
					if(dup2(pipeIn, 0) == -1){
						printf("reDirection standard input is error\n");
						return;
					}
				}        
				if(cmd->output != NULL){ //存在输出重定向
					if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
						printf("can not open the file %s\n", cmd->output);
						return ;
					}
					if(dup2(pipeOut, 1) == -1){
						printf("reDirection standard output is error\n");
						return;
					}
				}
			}
            if(cmd->isBack){ //son process is fg,father process is bg
                signal(SIGUSR1, setGoon); //收到信号，setGoon函数将goon置1，以跳出下面的循环
                while(goon == 0) ; //wait father's SIGUSR1 which can show that the job has been added in linklist
                goon = 0; //reset,for the next SIGUSR1
                
                printf("[%d]\t%s\t\t%s\n", getpid(), RUNNING, inputBuff);
                kill(getppid(), SIGUSR1);//send message to father process
            }
            else{
                signal(SIGUSR1, setGoon); //reseive the signal,the function setGoon lets goon be 1,in order to jump out of the loop
                while(goon == 0) ; //wait for father's SIGUSR1,which can show that fg and bg has been changed 
                goon = 0; //reset,for the next SIGUSR1
            }
            if (cmd->next==NULL){
				justArgs(cmd->args[0]);
				if(execv(cmdBuff, cmd->args) < 0){ //执行命令
					printf("execv failed!\n");
					return;
				}
			}
			else{
				executePipe(cmd);				
			}
        }
		else{ //father
			addJob(pid);
            if(cmd ->isBack){ //ºóÌšÃüÁî             
                fgPid = 0; //pidÖÃ0£¬ÎªÏÂÒ»ÃüÁî×ö×Œ±ž
         //       addJob(pid); //ÔöŒÓÐÂµÄ×÷Òµ
                signal(SIGUSR1, setGoon);
				  
                sleep(1);
               
                 kill(pid, SIGUSR1); //×Óœø³Ì·¢ÐÅºÅ£¬±íÊŸ×÷ÒµÒÑŒÓÈë  
                //µÈŽý×Óœø³ÌÊä³ö
                //signal(SIGUSR1, setGoon);
                while(goon == 0) ;
                goon = 0;
            }
            else{ //·ÇºóÌšÃüÁî
                fgPid = pid;

                if (tcsetpgrp(0,fgPid)<0)printf("0failed\n");   //转移控制
                if (tcsetpgrp(1,fgPid)<0)printf("1failed\n");
                if (tcsetpgrp(2,fgPid)<0)printf("2failed\n");

                sleep(1);
                kill(pid, SIGUSR1); //子进程发信号，表示控制已转移
                
                waitpid(pid, NULL, WUNTRACED);

                if (tcsetpgrp(0,getpid())<0)printf("0failed\n");    //回收控制
                if (tcsetpgrp(1,getpid())<0)printf("1failed\n");
                if (tcsetpgrp(2,getpid())<0)printf("2failed\n"); 
            }
		}
    }else{ //ÃüÁî²»ŽæÔÚ
        printf("can't find cmd%s\n", inputBuff);
    }
}

/*执行命令*/
void execSimpleCmd(SimpleCmd *cmd){
    int i, pid;
    char *temp;
    Job *now = NULL;
    struct passwd *pwd=getpwuid(getuid());
    if(strcmp(cmd->args[0], "exit") == 0) { //exitÃüÁî
        exit(0);
    } 
    else if(strcmp(cmd->args[0],"echo")==0){
       printf("%s\n",cmd->args[1]);
    }
    else if(strcmp(cmd->args[0],"pwd")==0){
       printf("%s\n",get_current_dir_name());
    }
    else if(strcmp(cmd->args[0],"whoami")==0){
       printf("%s\n",pwd->pw_name);
    }
    else if(strcmp(cmd->args[0],"tail")==0){
       int  buffersize = 20;  
       int argc=0;
       char *temp=cmd->args;
       //for(;temp!=NULL&strlen(temp)!=0;temp++){
       //    argc++;
       //}
       argc=length;
      // printf("%d",argc);
       if (argc ==1)  
          printf("arguments to short!\n");  
       else if(argc==2){
		  FILE *fp;
          if((fp=fopen(cmd->args[1],"r"))==NULL)
 	      {
			printf("The file <%s> can not be opened.\n",cmd->args[1]);
                 //打开操作不成功
                 return;
 	    }
   		 //成功打开了argv[1]所指文件
  	   char ch=fgetc(fp); //从fp所指文件的当前指针位置读取一个字符
  	   while(ch!=EOF) //判断刚读取的字符是否是文件结束符
	   {
 	       putchar(ch); //若不是结束符，将它输出到屏幕上显示
	        ch=fgetc(fp); //继续从fp所指文件中读取下一个字符
 	   } //完成将fp所指文件的内容输出到屏幕上显示
       }
       else{
       char *filename = cmd->args[3];  
       int num = 5;  
       if (argc == 4) 
          num = atoi((cmd->args)[2]);  
       FILE *file = fopen(filename, "r");  
       if ( file == NULL)  
          perror("file open error");  
       fseek(file, SEEK_SET, SEEK_END);  
       int filesize = ftell(file);  
       char res[5000];  
       char buffer[buffersize];  
       int now = 0, flag = 0;  
       int ith = 0;  
       int k, j = 0;  
       while (now < num && ftell(file))  
       {  
           if (ftell(file) < buffersize)  
               buffersize = ftell(file);  
           if ( fseek(file, -buffersize , SEEK_CUR) == -1)  
               perror("fseek error\n");  
           int nread = 0;  
           int n = 0;  
            while (nread != buffersize)  
            {  
               n = fread(buffer+nread, 1, buffersize-nread, file);  
               nread += n;  
            }  
             for (k = nread - 1; k >= 0; --k)  
             {  
                 if (buffer[k] == '\n')  
                 {  
                    ++now;  
                    if (now == num + 1)  
                   {  
                       flag = 1;  
                       break;  
                   }  
                }  
                res[ith++] = buffer[k];  
             }  
            if (flag)  
               break;  
           fseek(file, -buffersize , SEEK_CUR);  
       }  
       fclose(file);  
       for (k = ith; k >= 0; --k)  
       {  
            putchar(res[k]);  
       }
      }
    }
    else if (strcmp(cmd->args[0], "history") == 0) { //historyÃüÁî
        if(history.end == -1){
            printf("ÉÐÎŽÖŽÐÐÈÎºÎÃüÁî\n");
            return;
        }
        i = history.start;
        do {
            printf("%s\n", history.cmds[i]);
            i = (i + 1)%HISTORY_LEN;
        } while(i != (history.end + 1)%HISTORY_LEN);
     } 
     else if (strcmp(cmd->args[0], "jobs") == 0) { //jobsÃüÁî
        if(head == NULL){
            printf("ÉÐÎÞÈÎºÎ×÷Òµ\n");
        } 
        else {
            printf("index\tpid\tstate\t\tcommand\n");
            for(i = 1, now = head; now != NULL; now = now->next, i++){
                printf("%d\t%d\t%s\t\t%s\n", i, now->pid, now->state, now->cmd);
            }
        }
      } 
      else if (strcmp(cmd->args[0], "cd") == 0) { //cdÃüÁî
         temp = cmd->args[1];
         if(temp != NULL){
            if(chdir(temp) < 0){
                printf("cd; %s ŽíÎóµÄÎÄŒþÃû»òÎÄŒþŒÐÃû£¡\n", temp);
            }
        }
     } 
     else if (strcmp(cmd->args[0], "fg") == 0) { //fgÃüÁî
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            if(pid != -1){
                fg_exec(pid);
            }
      }
        else{
            printf("fg; ²ÎÊý²»ºÏ·š£¬ÕýÈ·žñÊœÎª£ºfg %<int>\n");
         }
      }
      else if (strcmp(cmd->args[0], "bg") == 0) { //bgÃüÁî
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));        
            if(pid != -1){
                bg_exec(pid);
            }
        }
		else{
            printf("bg; ²ÎÊý²»ºÏ·š£¬ÕýÈ·žñÊœÎª£ºbg %<int>\n");
        }
    } 
    else{ //外部命令
        execOuterCmd(cmd);
    }
    
    //释放结构体空间
    for(i = 0; cmd->args[i] != NULL; i++){
        free(cmd->args[i]);
        free(cmd->input);
        free(cmd->output);
    }
}

/*******************************************************
                    cmd interface
********************************************************/
void execute(){
	
    SimpleCmd *cmd = handleSimpleCmdStr(0, strlen(inputBuff));
    execSimpleCmd(cmd);
}
