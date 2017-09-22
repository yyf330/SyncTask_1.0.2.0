/*
 * =====================================================================================
 *
 *       Filename:  main.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  12/21/2015 11:42:29 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Dengbzh (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include"CThreadManager.h"
#include"CJob.h"

class mjob:public CJob
{
    void run(void *ptr){
        printf("Job %d!!\n",*((int *)ptr));
        sleep(1);
    }
};

int main()
{
    int jobnum = 200;
    CThreadManager *poolManager = new CThreadManager(10);
    int *data = new int[jobnum];    
    printf("Main func assign jobs...\n");
    for(int i=0;i<jobnum;i++)
    {
        data[i] = i;
        mjob *job = new mjob();
        poolManager->run(job,(void*)(data+i));
    }
   // sleep(50);
    printf("assign finished...\n");
    poolManager->terminateAll();
    delete data;
    return 0;
}
