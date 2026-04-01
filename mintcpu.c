#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define RAM_SIZE 0x400000
#define FB_ADDR 0xB0000
#define FB_W 1024
#define FB_H 768
#define FB_BPP 4

#define WIN_W 360
#define WIN_H 360

#define INST 8

typedef struct {
    uint32_t R[8];
    uint32_t PC;
    uint32_t FLAGS;
    int running;
} CPU;

CPU cpu;
uint8_t RAM[RAM_SIZE];

typedef struct {
    char name[64];
    uint32_t addr;
} Label;

Label labels[4096];
int label_count=0;

enum {
    NOP,
    MOV, ADD, SUB,
    CMP,
    JMP, JE, JNE,
    LOAD, STORE,
    HLT
};

void trim(char *s){
    while(*s==' '||*s=='\t') memmove(s,s+1,strlen(s));
    int l=strlen(s)-1;
    while(l>=0 && (s[l]=='\n'||s[l]=='\r'||s[l]==' '||s[l]=='\t')) { s[l]=0; l--; }
}

void remove_comment(char *s){
    char *c=strchr(s,';');
    if(c) *c=0;
}

int empty(char *s){
    for(int i=0;s[i];i++)
        if(s[i]!=' '&&s[i]!='\t') return 0;
    return 1;
}

uint32_t find_label(char *n){
    for(int i=0;i<label_count;i++)
        if(strcmp(labels[i].name,n)==0) return labels[i].addr;
    printf("Unknown label %s\n",n);
    exit(1);
}

int reg(char *t){ return t[1]-'0'; }

uint32_t val(char *t){
    if(strstr(t,"0x")) return strtoul(t,NULL,16);
    if(isalpha(t[0])) return find_label(t);
    return atoi(t);
}

void emit(uint32_t *pc,uint8_t op,uint8_t r1,uint8_t r2,uint8_t mode,uint32_t imm){
    RAM[*pc]=op;
    RAM[*pc+1]=r1;
    RAM[*pc+2]=r2;
    RAM[*pc+3]=mode;
    RAM[*pc+4]=imm;
    RAM[*pc+5]=imm>>8;
    RAM[*pc+6]=imm>>16;
    RAM[*pc+7]=imm>>24;
    *pc+=INST;
}

void assemble(char *file){
    label_count=0;
    FILE *f=fopen(file,"r");
    if(!f){ printf("File missing %s\n",file); exit(1); }
    char line[256];
    uint32_t pc=0;
    // first pass: collect labels
    while(fgets(line,256,f)){
        remove_comment(line); trim(line);
        if(empty(line)) continue;
        char *c=strchr(line,':');
        if(c){
            *c=0; trim(line);
            strcpy(labels[label_count].name,line);
            labels[label_count].addr=pc;
            label_count++;
            continue;
        }
        pc+=INST;
    }
    rewind(f);
    pc=0;
    // second pass: encode instructions
    while(fgets(line,256,f)){
        remove_comment(line); trim(line);
        if(empty(line)) continue;
        if(strchr(line,':')) continue;
        char *t=strtok(line," ,\t");
        if(!t) continue;
        if(strcmp(t,"MOV")==0){
            char *a=strtok(NULL," ,\t");
            char *b=strtok(NULL," ,\t");
            if(b[0]=='R') emit(&pc,MOV,reg(a),reg(b),0,0);
            else emit(&pc,MOV,reg(a),0,1,val(b));
            continue;
        }
        if(strcmp(t,"ADD")==0){
            char *a=strtok(NULL," ,\t"); char *b=strtok(NULL," ,\t");
            if(b[0]=='R') emit(&pc,ADD,reg(a),reg(b),0,0);
            else emit(&pc,ADD,reg(a),0,1,val(b));
            continue;
        }
        if(strcmp(t,"SUB")==0){
            char *a=strtok(NULL," ,\t"); char *b=strtok(NULL," ,\t");
            if(b[0]=='R') emit(&pc,SUB,reg(a),reg(b),0,0);
            else emit(&pc,SUB,reg(a),0,1,val(b));
            continue;
        }
        if(strcmp(t,"CMP")==0){
            char *a=strtok(NULL," ,\t"); char *b=strtok(NULL," ,\t");
            if(b[0]=='R') emit(&pc,CMP,reg(a),reg(b),0,0);
            else emit(&pc,CMP,reg(a),0,1,val(b));
            continue;
        }
        if(strcmp(t,"JMP")==0){ emit(&pc,JMP,0,0,1,val(strtok(NULL," \t"))); continue; }
        if(strcmp(t,"JE")==0){ emit(&pc,JE,0,0,1,val(strtok(NULL," \t"))); continue; }
        if(strcmp(t,"JNE")==0){ emit(&pc,JNE,0,0,1,val(strtok(NULL," \t"))); continue; }
        if(strcmp(t,"LOAD")==0){
            char *a=strtok(NULL," ,\t"); char *b=strtok(NULL," []\t");
            emit(&pc,LOAD,reg(a),reg(b),0,0); continue;
        }
        if(strcmp(t,"STORE")==0){
            char *a=strtok(NULL," ,\t"); char *b=strtok(NULL," []\t");
            emit(&pc,STORE,reg(a),reg(b),0,0); continue;
        }
        if(strcmp(t,"HLT")==0){ emit(&pc,HLT,0,0,0,0); continue; }
        printf("Unknown %s\n",t); exit(1);
    }
    fclose(f);
}

void exec(){
    uint32_t pc=cpu.PC;
    uint8_t op=RAM[pc]; uint8_t r1=RAM[pc+1]; uint8_t r2=RAM[pc+2]; uint8_t mode=RAM[pc+3];
    uint32_t imm = RAM[pc+4]|(RAM[pc+5]<<8)|(RAM[pc+6]<<16)|(RAM[pc+7]<<24);
    cpu.PC+=INST;
    switch(op){
        case MOV: cpu.R[r1] = mode? imm : cpu.R[r2]; break;
        case ADD: cpu.R[r1] += mode? imm : cpu.R[r2]; cpu.FLAGS=(cpu.R[r1]==0); break;
        case SUB: cpu.R[r1] -= mode? imm : cpu.R[r2]; cpu.FLAGS=(cpu.R[r1]==0); break;
        case CMP: cpu.FLAGS = mode? (cpu.R[r1]==imm) : (cpu.R[r1]==cpu.R[r2]); break;
        case JMP: cpu.PC=imm; break;
        case JE: if(cpu.FLAGS) cpu.PC=imm; break;
        case JNE: if(!cpu.FLAGS) cpu.PC=imm; break;
        case LOAD: cpu.R[r1]=*(uint32_t*)&RAM[cpu.R[r2]]; break;
        case STORE: *(uint32_t*)&RAM[cpu.R[r2]]=cpu.R[r1]; break;
        case HLT: cpu.running=0; break;
    }
}

SDL_Texture* tex;

void draw_fb(SDL_Renderer *r){
    void *pixels; int pitch;
    SDL_LockTexture(tex,NULL,&pixels,&pitch);
    memcpy(pixels,&RAM[FB_ADDR],FB_W*FB_H*4);
    SDL_UnlockTexture(tex);
    SDL_RenderCopy(r,tex,NULL,NULL);
}

void debug_print(){
    printf("\033[2JPC=%u FLAGS=%u\n",cpu.PC,cpu.FLAGS);
    for(int i=0;i<8;i++) printf("R%d=%08X ",i,cpu.R[i]);
    printf("\n");
}

int main(int argc,char **argv){
    char *file=NULL,*start=NULL;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-a")==0) file=argv[++i];
        if(strcmp(argv[i],"-l")==0) start=argv[++i];
    }
    if(!file){ printf("./mintcpu -a file.asm -l label\n"); return 0; }

    assemble(file);
    cpu.running=1;
    if(start) cpu.PC=find_label(start);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *disp=SDL_CreateWindow("MintCPU Display",100,100,WIN_W,WIN_H,0);
    SDL_Renderer *rdisp=SDL_CreateRenderer(disp,-1,0);

    tex = SDL_CreateTexture(rdisp,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,FB_W,FB_H);

    bool paused=0; bool app_running=1;

    while(app_running){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) app_running=0;
            if(e.type==SDL_KEYDOWN){
                if(e.key.keysym.sym==SDLK_ESCAPE) app_running=0;
                if(e.key.keysym.sym==SDLK_SPACE) paused=!paused;
                if(e.key.keysym.sym==SDLK_s){ exec(); debug_print(); }
                if(e.key.keysym.sym==SDLK_r){ memset(&cpu,0,sizeof(cpu)); assemble(file); cpu.running=1; if(start) cpu.PC=find_label(start); }
            }
        }
        if(!paused && cpu.running){
            for(int i=0;i<100000;i++) exec(); // batch instructions
        }

        SDL_SetRenderDrawColor(rdisp,0,0,0,255);
        SDL_RenderClear(rdisp);
        draw_fb(rdisp);
        SDL_RenderPresent(rdisp);

        SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}
