%{
    #include "global.h"

    int yylex ();
    void yyerror ();
    SimpleCmd* mEcho(SimpleCmd* head);

    SimpleCmd * mcmd;
    argC * argcv;
      
    int offset, len, commandDone;

%}

%token STRING
%token SP

%%
line            :   '\n' { lineIgnore = 0; mcmd = NULL; return 0;}
                    |command '\n'   {
                        if (lineIgnore) 
                        {
                            mcmd = NULL;
                            lineIgnore = 0;
                            return 0;
                        }
                        $$ = $1;
                        if ($$==NULL) return 0; 
                        //printf("line:%d\n",$$);
                        //printf("%s %s %s %s\n",mcmd->input,mcmd->output,argcv->s,argcv->next==NULL?"(null)":argcv->next->s); 
                        //strcpy(inputBuff,$1);   
                        //execute();  
                        for (mcmd = (SimpleCmd*)$$;mcmd!=NULL;mcmd = mcmd->next)
                        {
                            len = 0;
                            for (argcv = mcmd->ac;argcv!=NULL;argcv = argcv->next, ++len);
                            if (len) 
                            {
                                mcmd->argc = len;
                                mcmd->args = (char**)malloc(sizeof(char*)*len);
                                //mcmd->args[len] = NULL;
                                for (argcv = mcmd->ac;argcv!=NULL;argcv = argcv->next)
                                {
                                    mcmd->args[--len] = strdup(argcv->s);
                                }
                            }
                        }
                        mEcho((SimpleCmd*)$$);
                        mcmd = (SimpleCmd*)$$;
                        return 0; 
                    }
;

command         :   fbpCommand  {
                        $$ = $1;    
                        //printf("pC:%d\n",$$);
                    }
                    |fbCommand  {
                        $$ = $1;
                        //printf("sC:%d\n",$$);
                    }
;

fbpCommand      :   pCommand   { $$ = $1;}
                    |pCommand '&'  {
                        $$ = $1;  
                        mcmd = (SimpleCmd*)$$;
                        mcmd->isBack = 1;                        
                    }
;

pCommand        :   pCommand '|' simpleCmd {
                        $$ = $1;
                        for (mcmd = (SimpleCmd*)$$;mcmd->next!=NULL;mcmd = mcmd->next);
                        mcmd->next = $3;                                
                    }
                    |simpleCmd '|' simpleCmd {
                        $$ = $1;
                        mcmd = (SimpleCmd*)$$;
                        mcmd->next = $3;                                                
                    }
;

fbCommand       :   simpleCmd   { $$ = $1;}
                    |simpleCmd '&'  {
                        $$ = $1;  
                        mcmd = (SimpleCmd*)$$;
                        mcmd->isBack = 1;                        
                    }
;

simpleCmd       :   args inputRedirect outputRedirect {
                        $$ = $1;
                        //printf("sim:%d\n",$$);
                        //strcat($$,$2);
                        //strcat($$,$3);
                        mcmd = (SimpleCmd*)$$;
                        mcmd->input = $2;
                        mcmd->output = $3;
                    }
;

inputRedirect   :   /* empty */     {
                        $$ = NULL;
                    }
                    |'<' STRING     {
                        //$$ = strdup("<");
                        //strcat($$,$2);
                        $$ = strdup($2);
                    }
;

outputRedirect  :   /* empty */     {
                        $$ = NULL;
                    }
                    |'>' STRING     {
                        //$$ = strdup(">");
                        //strcat($$,$2);
                        $$ = strdup($2);
                    }
;

args            :   STRING     {
                        mcmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
                        mcmd->isBack = 0;
                        mcmd->argc = 0;
                        mcmd->input = NULL;
                        mcmd->output = NULL;
                        mcmd->next = NULL;
                        mcmd->ac = NULL;
                        mcmd->cmd = $1;
                        $$ = mcmd;
                        //printf("args:%d\n",$$);
                        mcmd = (SimpleCmd*)$$;
                        argcv = (argC*)malloc(sizeof(argC));
                        argcv->next = mcmd->ac;                            
                        argcv->s = strdup($1);
                        mcmd->ac = argcv;
                    }
                    |args STRING    {
                        //$$ = strdup($1);
                        //strcat($$," ");
                        //strcat($$,$2);
                        if ($1==NULL) 
                        {
                            mcmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));
                            mcmd->isBack = 0;
                            mcmd->argc = 0;
                            mcmd->input = NULL;
                            mcmd->output = NULL;
                            mcmd->next = NULL;
                            mcmd->ac = NULL;
                            $$ = mcmd;
                        }
                        else $$ = $1;
                        //printf("args:%d\n",$$);
                        mcmd = (SimpleCmd*)$$;
                        //argcv = (argC*)malloc(sizeof(argC));
                        //argcv->next = mcmd->ac;                            
                        //argcv->s = strdup($2);
                        //mcmd->ac = argcv;
                        extendArgs(mcmd,$2);
                    }
;

%%



/****************************************************************
                  词法分析函数
****************************************************************/

//由lex.l文件提供

/****************************************************************
                  错误信息执行函数
****************************************************************/
void yyerror()
{
    errFlag=1;
    if (lineIgnore) return;
    printf("你输入的命令不正确，请重新输入！\n");
}

/****************************************************************
                  main主函数
****************************************************************/
int main(int argc, char** argv) {
    int i;
    char c;

    commandDone = 0;
    offset = 0;
    len = 0;
    errFlag = 0;
    lineIgnore = 0;
    init(); //初始化环境
    
    printf("\n");
    printf("%s\n","/***********************************************************\\");
    printf("\n");
    printf("%s\n","*    Excited! Shell 1.0.0.0                                 *");
    printf("\n");
    printf("%s\n","*    Authors : Zhang Wei, Feng Weitao.                      *");
    printf("\n");
    printf("%s\n","*    Excited Studio (c) 2016-2016 Part of Rights Reserved.  *");
    printf("\n");
    printf("%s\n","\\***********************************************************/");

    while(1){

        printLine();

        if (mcmd!=NULL) free(mcmd);
        mcmd = NULL;
        if (argcv!=NULL) free(argcv);
        argcv = NULL;
  
        if (errFlag) lineIgnore = 1;
        errFlag=0;

        yyparse(); //调用语法分析函数，该函数由yylex()提供当前输入的单词符号

        //printf("%d %d\n",errFlag,lineIgnore);

        if (!errFlag&&mcmd!=NULL)
        {
            execSimpleCmd(mcmd);
            addHistory(inputBuff);
        }
        
     }

    return (EXIT_SUCCESS);
}
