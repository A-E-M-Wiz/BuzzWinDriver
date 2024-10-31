#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "buzzer.h"

int main(int argc, char *argv[])
{   
    int i,j;
    int buzzers;
    char buffer[BUZZER_MAXBUZZER];
    char buttons[BUZZER_MAXBUZZER];

    buzzers = BuzzerOpen();
    printf("BuzzerCount:%d\n",buzzers);

    i=0;
    while (!kbhit()) {
                             
        for (j=0;j<buzzers;j++) {
            buffer[j] = (i%buzzers)==j ? 1 : 0;
        }
        if (BuzzerSetLEDs(buffer,buzzers) || 0==buzzers) {
            BuzzerClose();
            buzzers = BuzzerOpen();
            printf("no buzzers found\n");
        } else if (BuzzerGetButtons(buttons,buzzers) || 0==buzzers) {
            BuzzerClose();
            buzzers = BuzzerOpen();
            printf("no buzzers found\n");
        } else {
            printf("%d/%d  ",i%buzzers+1,buzzers);
            for(j=0;j<buzzers;j++) {               
              printf("%2x ",buttons[j]);
            }               
            printf("\n");
        }
        sleep(200);
        
        if (buzzers <= ++i) i=0;
    }
    getch();     
    BuzzerClose();
    return 0;
}
