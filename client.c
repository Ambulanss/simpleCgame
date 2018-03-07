#include <stdio.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <memory.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>

#define ENDMSG_TYPE 666
#define PLAYER_STATUS 100
#define MSG_KEY 100
#define TEXT_TYPE 50
#define RECRUIT_TYPE 70
#define ATTACK_TYPE 80


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

void draw_borders(WINDOW *screen) {
    int x, y, i;
    getmaxyx(screen, y, x);
// 4 corners
    mvwprintw(screen, 0, 0, "+");
    mvwprintw(screen, y - 1, 0, "+");
    mvwprintw(screen, 0, x - 1, "+");
    mvwprintw(screen, y - 1, x - 1, "+");
// sides
    for (i = 1; i < (y - 1); i++) {
        mvwprintw(screen, i, 0, "|");
        mvwprintw(screen, i, x - 1, "|");
    }
// top and bottom
    for (i = 1; i < (x - 1); i++){
        mvwprintw(screen, 0, i, "-");
        mvwprintw(screen, y - 1, i, "-");
    }
}

int main( int argc, char ** argv){
    //printf("A\n");
    int my_id;
    player my_player;
    //printf("B\n");
    if(argc == 2 ) {
        //printf("C\n");
        my_id = atoi(argv[1]);
        //printf("D\n");
    }
    else
    {
        //printf("Wrong arguments.\n");
        return -1;
    }
    initscr();
    int parent_x, parent_y;
    int input_size = 3;
    noecho();
    //curs_set(FALSE);
    getmaxyx(stdscr,parent_y,parent_x);
    WINDOW *field = newwin(parent_y - input_size, parent_x, 0, 0);
    WINDOW *input = newwin(input_size,parent_x,parent_y-input_size,0);
    draw_borders(field);
    draw_borders(input);
    nodelay(input,TRUE);
    message ready;
    int my_queue = msgget(MSG_KEY+my_id,IPC_CREAT|0640);
    //printf("MSG_KEY+my_id: %d\t my_queue: %d\n",MSG_KEY+my_id,my_queue);
    ready.mtype = TEXT_TYPE+my_id;
    //printf("My mtype: %ld\n",ready.mtype);
    strcpy(ready.mtext, argv[1]);
    printf("%s\n",ready.mtext);
    msgsnd(my_queue,&ready,sizeof(ready)-sizeof(long),0);
    int endgame = 0;
    message end;
    message msg;
    /*if(!fork())
    {
        msgrcv(my_queue,&end,sizeof(end)-sizeof(end.mtype),ENDMSG_TYPE,IPC_NOWAIT);
        endgame = 1;
        clear();
        mvwprintw(field,0,0,"Player %s won the game!\n",end.mtext);
        wrefresh(field);
        sleep(3);

        exit(0);
    }*/
    strcpy(msg.mtext,"");
    int player_input;
    strcpy(end.mtext,"5");
    while(!endgame){
        nodelay(input, TRUE);
        draw_borders(input);
        msgrcv(my_queue,&end,sizeof(end)-sizeof(long int),ENDMSG_TYPE,IPC_NOWAIT);
        if(end.mtext[0] !='5' && end.mtext[0] != 'E')
        {
            endgame = 1;
            mvwprintw(field,9,2,"PLAYER %c WON THE GAME!\n",end.mtext[0]);
            wrefresh(field);
        }
        else
            if(end.mtext[0] == 'E')
            {
                endgame = 1;
                mvwprintw(field,9,2,"%s",end.mtext);
                wrefresh(field);
            }
        msgrcv(my_queue,&msg,sizeof(msg)-sizeof(long int), TEXT_TYPE,IPC_NOWAIT);
        msgrcv(my_queue,&my_player,sizeof(my_player)-sizeof(long),PLAYER_STATUS,0);
        clear();
        mvwprintw(field,1,2,"Player %d\n",my_id);
        mvwprintw(field,2,2,"Resources %d\n",my_player.resources);
        mvwprintw(field,3,2,"Workers %d\n",my_player.workers);
        mvwprintw(field,4,2,"Light infantry %d\n",my_player.military[0]);
        mvwprintw(field,5,2,"Heavy infantry %d\n",my_player.military[1]);
        mvwprintw(field,6,2,"Cavalry %d\n", my_player.military[2]);
        mvwprintw(field,7,2,"Victory points %d\n",my_player.VP);
        mvwprintw(field,8,2,"%s\n",msg.mtext);
        mvwprintw(field,11,2,"Press R to recruit\n");
        mvwprintw(field,12,2,"Press A to attack\n");
        mvwprintw(field,13,2,"Unit\tcost/att/def/time\n");
        mvwprintw(field,14,2,"Light infantry\t100/10/12/2\n");
        mvwprintw(field,15,2,"Heavy infantry\t250/15/30/3\n");
        mvwprintw(field,16,2,"Cavalry\t\t 550/35/12/5\n");
        mvwprintw(field,17,2,"Workers\t\t 150/0/0/2\t increase income by 5/s\n");
        draw_borders(field);
        draw_borders(input);
        wrefresh(field);
        wrefresh(input);

        player_input = mvwgetch(input, 1, 1);
        if(player_input)
        {
            if(player_input =='R'){
                nodelay(input,FALSE);
                wclear(input);

                draw_borders(input);
                wrefresh(input);
                units U;
                mvwprintw(input,0,1,"Light inf 0<X<10:");
                wrefresh(input);
                U.military[0] = (mvwgetch(input,1,1))-'0';
                mvwprintw(input,0,1,"Heavy inf 0<X<10:");
                wrefresh(input);
                U.military[1] = (mvwgetch(input,1,1))-'0';
                mvwprintw(input,0,1,"Cavalry 0<=X<10:");
                wrefresh(input);
                U.military[2] = (mvwgetch(input,1,1))-'0';
                mvwprintw(input,0,1,"Workers 0<X<10:");
                wrefresh(input);
                U.military[3] = (mvwgetch(input,1,1))-'0';
                U.mtype = RECRUIT_TYPE;
                for(int i = 0; i<4;i++)
                {
                    if(U.military[i]<0 || U.military[i] > 9)
                        U.military[i] = 0;
                }

                msgsnd(my_queue,&U,sizeof(U)-sizeof(long int),0);
                wrefresh(input);
                wclear(input);
                continue;
            }
            else
                if(player_input =='A'){
                    nodelay(input,FALSE);
                    aggro A;
                    A.mtype = ATTACK_TYPE;
                    mvwprintw(input,0,1,"Who?");
                    wrefresh(input);
                    A.who = (mvwgetch(input,1,1))-'0';
                    mvwprintw(input,0,1,"Light inf 0<X<10:");
                    wrefresh(input);
                    A.army[0] = (mvwgetch(input,1,1))-'0';
                    mvwprintw(input,0,1,"Heavy inf 0<X<10:");
                    wrefresh(input);
                    A.army[1] = (mvwgetch(input,1,1))-'0';
                    mvwprintw(input,0,1,"Cavalry 0<X<10:");
                    wrefresh(input);
                    A.army[2] = (mvwgetch(input,1,1))-'0';
                    for(int i = 0; i < 3; i++)
                    {
                        if(A.army[i] < 0 || A.army[i] > 9)
                        {
                            A.army[i] = 0;
                        }
                    }
                    if(!(A.who == my_id || A.who < 0 || A.who > 2))
                    {
                        msgsnd(my_queue,&A,sizeof(A)-sizeof(long int), 0);
                    }
                    else
                    {
                        mvwprintw(field,8,2,"You have chosen wrong target.\n");
                        wrefresh(field);
                    }
                    wrefresh(input);
                    wclear(input);
                    continue;
                } else{
                    wrefresh(input);
                    wclear(input);
                    continue;}

        }
    }
    sleep(3);
    delwin(field);
    delwin(input);
    endwin();
    return 0;

}
