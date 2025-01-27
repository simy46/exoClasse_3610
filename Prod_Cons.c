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
* File : RDVUni_Queue_SemCompteur.c
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


// Le jour ou on voudra mettre TaskDevFifo en vrai ISR la declaration des p�riph�riques seront deja la...
XIntc axi_intc;
XGpio gpSwitch;

#define GIC_DEVICE_ID	        XPAR_PS7_SCUGIC_0_DEVICE_ID
#define PL_INTC_IRQ_ID          XPS_IRQ_INT_ID
#define FIT_1S_IRQ_ID           XPAR_AXI_INTC_0_FIT_TIMER_0_INTERRUPT_INTR
#define FIT_3S_IRQ_ID			XPAR_AXI_INTC_0_FIT_TIMER_1_INTERRUPT_INTR
#define GPIO_SW_IRQ_ID			XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define GPIO_SW_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID

#define XGPIO_IR_MASK		0x3 /**< Mask of all bits */

// � utiliser pour suivre le remplissage et le vidage des fifos
// Mettre en commentaire et utiliser la fonction vide suivante si vous ne voulez pas de trace
#define safeprintf(fmt, ...)															\
{																						\
	OSMutexPend(&Mutex_print, 0, OS_OPT_PEND_BLOCKING, &ts, &err);						\
	xil_printf(fmt, ##__VA_ARGS__);														\
	OSMutexPost(&Mutex_print, OS_OPT_POST_NONE, &err);									\
}

// � utiliser pour ne pas avoir les traces de remplissage et de vidage des fifos
//#define safeprintf(fmt, ...)															\
//{																						\
//}


/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define				TASK_STK_SIZE			8192                // Size of each task's stacks (# of WORDs)
#define				N						256					// Size of the home made message queue

#define          	TaskCons1Prio     		20
#define          	TaskCons2Prio    		21
#define          	TaskCons3Prio    		22
#define          	TaskCons4Prio    		23
#define          	TaskCons5Prio    		24

#define          	TaskProdPrio    		30

#define 				All_Tasks  				0x01


/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

CPU_STK          	TaskCons1Stk[TASK_STK_SIZE];
CPU_STK          	TaskCons2Stk[TASK_STK_SIZE];
CPU_STK          	TaskCons3Stk[TASK_STK_SIZE];
CPU_STK          	TaskCons4Stk[TASK_STK_SIZE];
CPU_STK          	TaskCons5Stk[TASK_STK_SIZE];

CPU_STK           	TaskProdStk[TASK_STK_SIZE];

CPU_STK 		 	StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

OS_TCB            TaskCons1TCB;
OS_TCB            TaskCons2TCB;
OS_TCB            TaskCons3TCB;
OS_TCB            TaskCons4TCB;
OS_TCB            TaskCons5TCB;

OS_TCB            TaskProdTCB;

OS_TCB			 StartupTaskTCB;

OS_FLAG_GRP 		 		FlagGroup;

OS_MUTEX Mutex, Mutex_done, Mutex_print;
OS_SEM SemFull;
OS_SEM SemEmpty;

int buffer[N];

int add, rem = 0;

int Producer_done = 0;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void             TaskCons(void* data);               // Function prototypes of tasks
void             TaskProd(void* data);
void 			 StartupTask (void *p_arg);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main (void)
{
    OS_ERR err;

    UCOS_LowLevelInit();

    CPU_Init();
    Mem_Init();
    OSInit(&err);

    OSTaskCreate(&TaskCons1TCB, "TaskCons1", TaskCons, (void*)0, TaskCons1Prio,
        &TaskCons1Stk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    OSTaskCreate(&TaskCons2TCB, "TaskCons2", TaskCons, (void*)0, TaskCons2Prio,
        &TaskCons2Stk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    OSTaskCreate(&TaskCons3TCB, "TaskCons3", TaskCons, (void*)0, TaskCons3Prio,
        &TaskCons3Stk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    OSTaskCreate(&TaskCons4TCB, "TaskCons4", TaskCons, (void*)0, TaskCons4Prio,
        &TaskCons4Stk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    OSTaskCreate(&TaskCons5TCB, "TaskCons5", TaskCons, (void*)0, TaskCons5Prio,
        &TaskCons5Stk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    OSTaskCreate(&TaskProdTCB, "TaskProd", TaskProd, (void*)0, TaskProdPrio,
        &TaskProdStk[0u], TASK_STK_SIZE / 2, TASK_STK_SIZE, 20, 0, (void*)0,
        (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSMutexCreate(&Mutex, "Mutex", &err);
    OSMutexCreate(&Mutex_done, "Mutex_done", &err);

    OSMutexCreate(&Mutex_print, "Mutex_print", &err);

    OSSemCreate(&SemFull, "SemFull", 0, &err);
    OSSemCreate(&SemEmpty, "SemEmpty", N, &err);

	OSFlagCreate(&FlagGroup, "Flag Group", (OS_FLAGS)0, &err);

    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               TASKS
*********************************************************************************************************
*/

void  TaskCons(void* pdata)
{
    OS_ERR err;
    CPU_TS ts;

    int out;

    pdata = pdata;
    safeprintf("Cons no %d: demarre\n", OSPrioCur);
    while (1) {

    		OSFlagPend(&FlagGroup, All_Tasks, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

        OSMutexPend(&Mutex_done, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
        if (Producer_done) {								// Le producteur a-t-il termin� sa production
            OSMutexPost(&Mutex_done, OS_OPT_POST_NONE, &err);
            OSSemPend(&SemFull, 0, OS_OPT_PEND_NON_BLOCKING, &ts, &err);  // On prend soin de ne pas rest� bloqu�
            if (err == OS_ERR_PEND_WOULD_BLOCK) {
                safeprintf("\nCons no %d: va se detruire\r\n", OSPrioCur);
                OSTaskDel((OS_TCB*)0, &err);		                // Si il n'y a plus rien � consommer
                                                                    // c'est la fin!
            }
            else {
                OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);	// sinon on consomme
                out = buffer[rem++];										//memcpy
                if (rem >= N)
                    rem = 0;
                OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);
                OSSemPost(&SemEmpty, OS_OPT_POST_1, &err);
                safeprintf("\nCons appel sans delai no %d:  %d\r\n", OSPrioCur, out);
                OSTimeDly(100, OS_OPT_TIME_DLY, &err);
            }
        }
        else { 											  // Tout fonctionne normalement
            OSMutexPost(&Mutex_done, OS_OPT_POST_NONE, &err);
            safeprintf("\nCons no %d: Va se mettre en attente de SemFull\r\n", OSPrioCur);
            OSSemPend(&SemFull, 100, OS_OPT_PEND_BLOCKING, &ts, &err);	// Pour �viter de rester pris ind�finimment, on attend 100 ticks

            if (err != OS_ERR_TIMEOUT) {
                OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
                out = buffer[rem++];
                if (rem >= N)
                    rem = 0;
                OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);
                OSSemPost(&SemEmpty, OS_OPT_POST_1, &err);
                safeprintf("\nCons appel avec delai no %d:  %d\r\n", OSPrioCur, out);
                OSTimeDly(100, OS_OPT_TIME_DLY, &err);
            }
        }

    }
}


void  TaskProd(void* data)
{
    OS_ERR err;
    CPU_TS ts;

    int i, in = 0;
    data = data;

    safeprintf("Prod %d: demarre\n", OSPrioCur);

    while (1) {
    		OSFlagPend(&FlagGroup, All_Tasks, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

        for (i = 1; i <= 7; i++) {       // Il s'agit de v�rifier si les 2 t�ches qui n'auront pas de slot
                                                   // vont rester bloqu�es. Le cas devrait �tre g�n�ralisable pour
                                                   // toute valeur qui n'est pas modulo 10.

            OSSemPend(&SemEmpty, 0, OS_OPT_PEND_BLOCKING, &ts, &err); // Acquire semaphore
            OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
            buffer[add++] = in++;			//memcpy
            if (add >= N)
                add = 0;
            safeprintf("\nProduction de la valeur %d:\r\n", (in - 1));
            OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);			// Release semaphore
            OSSemPost(&SemFull, OS_OPT_POST_1, &err);
        }
        safeprintf("\nAttention: fin de la Production\r\n");
        OSMutexPend(&Mutex_done, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
        Producer_done = 1;
        OSMutexPost(&Mutex_done, OS_OPT_POST_NONE, &err);
        OSTaskDel((OS_TCB*)0, &err);
    }
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

#if (APP_OSIII_ENABLED == DEF_ENABLED)
#if (OS_CFG_STAT_TASK_EN == DEF_ENABLED)
    OSStatTaskCPUUsageInit(&os_err);
#endif
#endif

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

    OSFlagPost(&FlagGroup, All_Tasks , OS_OPT_POST_FLAG_SET, &err);

    OSTimeDlyHMSM(1,0,0,0, OS_OPT_TIME_DLY, &err);

    UCOS_Print("Prepare to shutdown System - \r\n");

    OSFlagPost(&FlagGroup, All_Tasks, OS_OPT_POST_FLAG_CLR, &err);

    OSTaskSuspend((OS_TCB *)0,&err);

}





