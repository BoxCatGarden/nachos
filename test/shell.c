#include "syscall.h"

int main()
{
    SpaceId newProc;
    OpenFileId input = ConsoleInput;
    OpenFileId output = ConsoleOutput;
    char prompt[2], ch = '\n', buffer[60];
    int i;
    //for(i=0;i<60;++i)buffer[i]=0;
    prompt[0] = '-';
    prompt[1] = '-';

    while (1)
    {
        Write(prompt, 2, output);

        if (i = Read(buffer, 59, input))
            --i;
        
//Write(&ch, 1, output);

        if (i > 0)
        {
            buffer[i] = '\0';
            if (!Strncmp(buffer, "exit", 4))
                Halt();
            newProc = Exec(buffer);
            Join(newProc);
        }
    }
}
