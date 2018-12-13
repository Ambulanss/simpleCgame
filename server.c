#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <memory.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define ENDMSG_TYPE 666
#define ENDGAME_KEY 252452
#define READY_KEY 252453
#define PLAYER_STATUS 100
#define SHM_KEY 987655
#define SEM_KEY 987654
#define MSG_KEY 100
#define TEXT_TYPE 50
#define RECRUIT_TYPE 70
#define ATTACK_TYPE 80
#define RECRUIT_KEY 75423

int signal_happened = 0;

struct sembuf p = {0, -1, 0};
struct sembuf v = {0, 1, 0};


typedef struct Message{
    long mtype;
    char mtext[128];
}message;

typedef struct Player {
    long mtype;
    int resources;
    int workers;
    int VP;
    int military[3];
}player;

typedef struct Units {
    long mtype;
    int military[4];
}units;

typedef struct Aggro{
    long mtype;
    int army[3];
    int who;
}aggro;

typedef struct recruit {
    long mtype;
    int unit_id;
}recru;

void send_end(int msgids[3],int id, int * endgame){
    message end;
    *endgame = 1;
    end.mtype = ENDMSG_TYPE;
    char buf[128]= {};
    if(id >= 0){
        buf[0] = (char)(id +'0');
        strcpy(end.mtext,buf);
    }
    else{
        strcpy(end.mtext,"Emergency server shutdown.\n");
    }
    for(int i = 0; i < 3; i++)
    {
        msgsnd(msgids[i],&end,sizeof(end)-sizeof(long int),0);
    }


}

void remove_all(int sems[3], int queues[3], int shmids[3])
{
    for(int i = 0; i < 3; i++)
    {
        msgctl(queues[i], IPC_RMID,0);
        semctl(sems[i],0,IPC_RMID,0);
        shmctl(shmids[i],IPC_RMID,0);
    }
    int a = msgget(RECRUIT_KEY,0);
    msgctl(a,IPC_RMID,0);
    printf("Queues, shared memory and semaphores removed.\n");
}

void mf(int a)
{
    signal_happened = 1;
}

void send_text(int msgid, char * text)
{
    message M;
    M.mtype = TEXT_TYPE;
    strcpy(M.mtext,text);
    msgsnd(msgid,&M,sizeof(M)-sizeof(long int),0);
}

void update_player(player * me, int semid, int msgid)
{

    if(semop(semid, &p, 1) == -1) 
    {
        perror("Semaphores are disturbed, further data integrity cannot be guaranteed.\n"); 
        //TODO add perrors to all semaphores, or make a wrapper for semop
        //It turns out this program ran by accident
        //Serious bugfix thanks to Merecatt <3
    }
    printf("Update resources: %d\n", me->resources);
    msgsnd(msgid, me,sizeof(*me)-sizeof(long int),0);
    semop(semid, &v, 1);
}

void update_resources(int semid, player * me)
{
    semop(semid, &p, 1);
    //printf("Resources before: %d\n", me->resources);
    me->resources += 50 + 5 * me->workers;
    //printf("Resources after: %d\n", me->resources);
    semop(semid,&v,1);
}

int costs(units army)
{
    int sum = 0;
    int costs[4] = {100, 250, 550, 150};
    for(int i = 0; i < 4; i++)
    {
        sum+=army.military[i]*costs[i];
    }
    return sum;
}

void recruit(player * me,int player_id,int my_sem, int my_msg, int recruitqueue, int * endgame){
    units A;
    while(*endgame == 0) {
        msgrcv(my_msg, &A, sizeof(A) - sizeof(long int), RECRUIT_TYPE, 0);
        //printf("Received: %d %d %d %d\n", A.military[0], A.military[1],A.military[2],A.military[3]);
        semop(my_sem, &p, 1);
        if (costs(A) <= me->resources) {
            me->resources -= costs(A);
            semop(my_sem, &v, 1);
            for (int i = 0; i < 4; i++) {
                while (A.military[i] > 0) {
                    recru R;
                    R.mtype = RECRUIT_TYPE + player_id;
                    R.unit_id = i;
                    msgsnd(recruitqueue, &R, sizeof(R) - sizeof(long int), 0);
                    A.military[i]-=1;
                }
            }
        } else {
            send_text(my_msg, "We need more resources.\n");
            printf("Recruit request denied.\n");
            semop(my_sem, &v, 1);
        }
    }
    exit(0);
}

void addUnits(player * me,int player_id, int my_sem, int recruitqueue, int * endgame)
{
    unsigned times[4] = {2, 3, 5, 2};
    recru Q;
    while(*endgame == 0) {
        msgrcv(recruitqueue, &Q, sizeof(Q) - sizeof(long int), RECRUIT_TYPE + player_id, 0);
        sleep(times[Q.unit_id]);
        semop(my_sem, &p, 1);
        if (Q.unit_id < 3) {
            me->military[Q.unit_id] += 1;
        } else {
            me->workers += 1;
        }
        semop(my_sem, &v, 1);
    }
    exit(0);
}
void battle(player * players[3],int atk_id,int def_id, int sems[3], aggro A, int msgs[3]){
    int off[3] = {10, 15, 35};
    int def[3] = {12,30,12};
    int sum_off = 0;
    int sum_def = 0;
    message todef;
    message toatk;
    todef.mtype = TEXT_TYPE;
    toatk.mtype = TEXT_TYPE;
    sleep(5);
    semop(sems[def_id],&p,1);
    for(int i = 0; i < 3; i++){
        sum_off += off[i]*A.army[i];
        sum_def += def[i]*players[def_id]->military[i];
    }
    semop(sems[atk_id],&p,1);
    if(sum_off > sum_def){

        for(int i = 0; i < 3; i++){
            if(sum_def>0) {
                A.army[i] = (int) ((float) A.army[i] * (float) sum_def / (float) sum_off);
                players[def_id]->military[i] = 0;
                players[atk_id]->military[i] += A.army[i];
            }
            else
            {
                players[atk_id]->military[i]+=A.army[i];
            }
        }
        strcpy(todef.mtext,"We've been attacked and lost :(\n");
        strcpy(toatk.mtext,"We attacked and won >:D\n");
        players[atk_id]->VP++;
    }
    else
    {
        for(int i = 0; i < 3; i++){
            A.army[i] =(int)((float)A.army[i] * (float)sum_off/(float)sum_def);
            players[def_id]->military[i] =(int)((float)players[def_id]->military[i] * (float)sum_off/(float)sum_def);
            players[atk_id]->military[i] +=A.army[i];
        }
        strcpy(todef.mtext,"We've been attacked and won >:D\n");
        strcpy(toatk.mtext,"Our attack failed :(\n");
    }
    semop(sems[atk_id],&v,1);
    semop(sems[def_id],&v,1);
    msgsnd(msgs[atk_id],&toatk,sizeof(toatk)-sizeof(long int), 0);
    msgsnd(msgs[def_id],&todef,sizeof(todef)-sizeof(long int), 0);
}
void attack(player * players[3],int my_id, int sems[3],int msgs[3],int * endgame){
    aggro U;
    while(*endgame==0) {
        msgrcv(msgs[my_id], &U, sizeof(U) - sizeof(long int), ATTACK_TYPE,0);
        semop(sems[my_id],&p,1);
        int dont = 0;
        for(int i = 0; i < 3; i++)
        {
            if(U.army[i] > players[my_id]->military[i])
            {
                send_text(msgs[my_id],"We need more soldiers.\n");
                semop(sems[my_id],&v,1);
                dont = 1;
            }

        }
        if(dont == 1) continue;
        for(int i = 0; i < 3; i++)
        {
            players[my_id]->military[i] -= U.army[i];
        }
        if(!fork()) {
            battle(players, my_id, U.who, sems, U, msgs);
            exit(0);
        }
    }
}


void player_handling(player * players[3],int my_id, int sems[3], int msgs[3], int * endgame)
{
    //printf("player_handling resources: %d\n",*me);
    int recruitqueue = msgget(RECRUIT_KEY,IPC_CREAT|0640);
    if(!fork()){
        while(*endgame == 0){
            sleep(1);
            update_resources(sems[my_id],players[my_id]);
        }
        exit(0);
    }
    if(!fork()){
        while(*endgame == 0){
            sleep(1);
            update_player(players[my_id], sems[my_id], msgs[my_id]);
        }
        exit(0);
    }
    if(!fork())
    {
        recruit(players[my_id], my_id, sems[my_id], msgs[my_id], recruitqueue, endgame);
    }
    if(!fork())
    {
        addUnits(players[my_id],my_id,sems[my_id],recruitqueue,endgame);
    }
    if(!fork())
    {
        attack(players,my_id,sems,msgs,endgame);
    }
}

void test(player * P, int sem){
    semop(sem,&p,1);
    P->resources += 50;
    semop(sem,&v,1);
}

void players_init(player * players[3], int sems[3], int queues[3], int * endgame, int * all)
{
    for(int i = 0; i < 3; i++)
    {
        message m;
        //printf("Init queue[%d]: %d\n",i,queues[i]);
        //printf("Init resources of %d: %d\n",i, players[i]->resources);
        if(!fork()){
            if(msgrcv(queues[i],&m,sizeof(m),TEXT_TYPE+i,0) == -1)
            {
                printf("%s\n",strerror(errno));
                exit(0);
            }
            else{
                semop(sems[0],&p,1);
                *all+=1;
                semop(sems[0],&v,1);
            } 
            //printf("Message received.\n");
            while((*all != 3));
            printf("Let the game begin!\n");
            if(!*endgame) {
                player_handling(players, i, sems, queues, endgame);
            }
            exit(0);
        }
    }

}



int main(){
    player * players[3];
    int semaphores[3];
    int playqueues[3];
    int shmids[3];
    int * endgame;
    int end_id = shmget(ENDGAME_KEY, sizeof(int),IPC_CREAT|0640);
    endgame = shmat(end_id,0,0);
    *endgame = 0;
    int ready_id = shmget(READY_KEY, sizeof(int), IPC_CREAT|0640);
    int *all_ready;
    all_ready = shmat(ready_id,0,0);
    *all_ready = 0;

    for(int i = 0; i < 3; i++){

        semaphores[i] = semget(SEM_KEY+i, 1, IPC_CREAT|0640);
        semctl(semaphores[i], 0, SETVAL, 1);

        shmids[i] = shmget(SHM_KEY+i, sizeof(player),IPC_CREAT|0640);
        players[i] = shmat(shmids[i],0,0);
        players[i]->resources = 300;
        players[i]->mtype = PLAYER_STATUS;
        players[i]->workers = 0;
        players[i]->VP = 0;

        playqueues[i] = msgget(MSG_KEY+i,IPC_CREAT|0640);
        printf("MSG_KEY+i: %d\t msg_id: %d\n", MSG_KEY+i, playqueues[i]);
        for(int j = 0; j < 3; j++){
            players[i]->military[j] = 0;
        }
    }

    for(int i = 0; i < 3; i++)
    {
        printf("Sem[%d]: %d\n",i,semaphores[i]);
        printf("Queue[%d]: %d\n",i, playqueues[i]);
        printf("Resources of i: %d\n",players[i]->resources);
    }
    //signal(2, signal_reaction(semaphores,playqueues,shmids, endgame));
    signal(2,mf);
    //test(players[2],semaphores[2]);
    //printf("Size of player: %d\t Size of *player: %d\n",(int)sizeof(players[0]),(int)sizeof(*players[0]));
    players_init(players, semaphores, playqueues, endgame, all_ready);
    //printf("%d\n", players[2]->resources);
    if(!fork())
    {
        while(*endgame==0 && signal_happened == 0);
        if(signal_happened){
            send_end(playqueues,-1,endgame);
        }
        exit(0);
    }
    if(!fork()) {
        while (*endgame == 0) {
            for (int i = 0; i < 3; i++) {
                semop(semaphores[i], &p, 1);
                if(players[i]->VP == 5)
                {

                    send_end(playqueues,i, endgame);
                }
                semop(semaphores[i],&v,1);
            }
        }
        exit(0);
    }
    while(*endgame == 0);
    sleep(10);
    remove_all(semaphores, playqueues, shmids);
    return 0;
}