/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*                                                  07/2020
*
* File : mutex.c
*
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <lib_mem.h>

#include <os.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include <app_cfg.h>
#include <cpu.h>
#include <ucos_bsp.h>
#include <ucos_int.h>
#include <xparameters.h>
#include <KAL/kal.h>

#include <xil_printf.h>

#include  <stdio.h>
#include  <ucos_bsp.h>

#include <Source/os.h>
#include <os_cfg_app.h>

#include <xgpio.h>
#include <xintc.h>
#include <xil_exception.h>



/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define         TASK_STK_SIZE    8192                // Size of each task's stacks (# of WORDs)


#define         TASK_B_PRIO    	20                // Application tasks priorities
#define			TASK_M_PRIO		18
#define			TASK_H_PRIO		16


/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

OS_PRIO My_Prio = 0;

CPU_STK         Task_B_Stk[TASK_STK_SIZE];     	// Startup    task stack
CPU_STK			Task_M_Stk[TASK_STK_SIZE];
CPU_STK			Task_H_Stk[TASK_STK_SIZE];
CPU_STK 		StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

OS_TCB			Task_B_TCB;
OS_TCB			Task_M_TCB;
OS_TCB			Task_H_TCB;
OS_TCB 			StartupTaskTCB;

  /* This disables all code until the next "#endif" */

OS_MUTEX		Mutex;
OS_SEM			Sem1, Sem2;			// les semaphores emulent le reveil des t�ches


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void            Task_B(void *data);          	// Tache de basse priorit�
void			Task_M(void *data);				// Tache de priorit� milieu
void			Task_H(void *data);				// tache haute priorit�

void 			StartupTask (void *p_arg);
void 			PrintOwnerofMutexanditsPriority(char *str, OS_MUTEX  *p_mutex);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/


int main (void)
{

 	OS_ERR  err;

    UCOS_LowLevelInit();

    CPU_Init();
    Mem_Init();
    OSInit(&err);


	OSMutexCreate(&Mutex, "Mutex", &err);
	OSSemCreate(&Sem1, "Sem1", 0, &err);
	OSSemCreate(&Sem2, "Sem2", 0, &err);

	OSTaskCreate(&Task_B_TCB,"Task_Basse_Priorite", Task_B, (void *) 0, TASK_B_PRIO,
       &Task_B_Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&Task_M_TCB,"Task_Milieu_Priorite", Task_M, (void *) 0, TASK_M_PRIO,
       &Task_M_Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&Task_H_TCB,"Task_Haute_Priorite", Task_H, (void *) 0, TASK_H_PRIO,
       &Task_H_Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               TASKS
*********************************************************************************************************
*/

void  Task_B (void *data)

{
	OS_ERR  err;
	CPU_TS ts;
	while(1)
	{
		UCOS_Print("\nTB - Avant le muxtex \r\n");
		PrintOwnerofMutexanditsPriority("TB", &Mutex);
		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		UCOS_Print("\nTB - Apres mutex \r\n");
		PrintOwnerofMutexanditsPriority("TB", &Mutex);
		UCOS_Print("\nTB - Avant Sem1 \r\n");
		OSSemPost(&Sem1, OS_OPT_POST_1, &err);
		UCOS_Print("\nTB - Apres Sem1 \r\n");
		PrintOwnerofMutexanditsPriority("TB", &Mutex);
		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);
		UCOS_Print("TB - Apres mutex \r\n");
		OSTimeDlyHMSM(1,0,0,0, OS_OPT_TIME_DLY, &err);
	}
}

void Task_M (void *data)
{
	OS_ERR  err;
	CPU_TS ts;

	while(1)
	{
		UCOS_Print("\nTM - Avant Sem1 \r\n");
		OSSemPend(&Sem1, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		UCOS_Print("\nTM - Apres Sem1 \r\n");
		OSSemPost(&Sem2, OS_OPT_POST_1, &err);
		UCOS_Print("\nTM - Apres Sem2 \r\n");
		OSTimeDlyHMSM(1,0,0,0, OS_OPT_TIME_DLY, &err);
	}
}

void Task_H(void *data)
//Task high
{
	OS_ERR  err;
	CPU_TS ts;

	int cnt = 0;
	while(1)
	{
		UCOS_Print("\nTH - Avant Sem2 \r\n");
		OSSemPend(&Sem2,0, OS_OPT_PEND_BLOCKING, &ts, &err);
		UCOS_Print("\nTH - Apres Sem2 et avant Mutex \r\n");
		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		UCOS_Print("\nTH - Apres Mutex \r\n");
		PrintOwnerofMutexanditsPriority("TH", &Mutex);
		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);
		PrintOwnerofMutexanditsPriority("TH", &Mutex);
		OSTimeDlyHMSM(1,0,0,0, OS_OPT_TIME_DLY, &err);
	}


}


void PrintOwnerofMutexanditsPriority(char *str, OS_MUTEX  *p_mutex)
{
		if (p_mutex->OwnerNestingCtr != 0u){                    /* Resource available?                                  */
		xil_printf("\nJe suis actuellement dans: %s\n", str);
		xil_printf("Le mutex est utilise par la tache : %s\n", p_mutex->OwnerTCBPtr->NamePtr);
		xil_printf("et la priorite courante de cette tache est : %d\n", p_mutex->OwnerTCBPtr->Prio);
		}
		else xil_printf("\nLe mutex est libre\n");
}

/*
*********************************************************************************************************
*                                               STARTUP TASK
*********************************************************************************************************
*/

void StartupTask (void *p_arg)
{
	int i;
	OS_ERR err;
    KAL_ERR kal_err;
    CPU_INT32U tick_rate;
#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    MEM_SEG_INFO seg_info;
    LIB_ERR lib_err;
#endif
#if (APP_OSIII_ENABLED == DEF_ENABLED)
#if (OS_CFG_STAT_TASK_EN == DEF_ENABLED)
    OS_ERR  os_err;
#endif
#endif

    UCOS_IntInit();

#if (APP_OSIII_ENABLED == DEF_ENABLED)
    tick_rate = OS_CFG_TICK_RATE_HZ;
#endif

#if (APP_OSII_ENABLED == DEF_ENABLED)
    tick_rate = OS_TICKS_PER_SEC;
#endif

    UCOS_TmrTickInit(tick_rate);                                /* Configure and enable OS tick interrupt.              */

//#if (APP_OSIII_ENABLED == DEF_ENABLED)
//#if (OS_CFG_STAT_TASK_EN == DEF_ENABLED)
//    OSStatTaskCPUUsageInit(&os_err);
//#endif
//#endif

    KAL_Init(DEF_NULL, &kal_err);
    UCOS_StdInOutInit();
    UCOS_PrintfInit();


#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    UCOS_Print("UCOS - uC/OS Init Started.\r\n");
    UCOS_Print("UCOS - STDIN/STDOUT Device Initialized.\r\n");
#endif

#if (APP_SHELL_ENABLED == DEF_ENABLED)
    UCOS_Shell_Init();
#endif

#if ((APP_FS_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_FS == DEF_ENABLED))
    UCOS_FS_Init();
#endif

#if ((APP_TCPIP_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_NET == DEF_ENABLED))
    UCOS_TCPIP_Init();
#endif /* (APP_TCPIP_ENABLED == DEF_ENABLED) */

#if ((APP_USBD_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_USBD == DEF_ENABLED) && (UCOS_USB_TYPE == UCOS_USB_TYPE_DEVICE))
    UCOS_USBD_Init();
#endif /* #if (APP_USBD_ENABLED == DEF_ENABLED) */

#if ((APP_USBH_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_USBH == DEF_ENABLED) && (UCOS_USB_TYPE == UCOS_USB_TYPE_HOST))
    UCOS_USBH_Init();
#endif /* #if (APP_USBH_ENABLED == DEF_ENABLED) */

#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    Mem_SegRemSizeGet(DEF_NULL, 4, &seg_info, &lib_err);

    UCOS_Printf ("UCOS - UCOS init done\r\n");
    UCOS_Printf ("UCOS - Total configured heap size. %d\r\n", seg_info.TotalSize);
    UCOS_Printf ("UCOS - Total used size after init. %d\r\n", seg_info.UsedSize);
#endif

    UCOS_Print("Programme initialise -\r\n");

    OSTaskSuspend((OS_TCB *)0,&err);

}


