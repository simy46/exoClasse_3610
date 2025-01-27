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
* File : no34.c
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

XIntc axi_intc;
XGpio gpSwitch;

#define GIC_DEVICE_ID	        XPAR_PS7_SCUGIC_0_DEVICE_ID
#define PL_INTC_IRQ_ID          XPS_IRQ_INT_ID
#define FIT_1S_IRQ_ID           XPAR_AXI_INTC_0_FIT_TIMER_0_INTERRUPT_INTR
#define FIT_3S_IRQ_ID			XPAR_AXI_INTC_0_FIT_TIMER_1_INTERRUPT_INTR
#define GPIO_SW_IRQ_ID			XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define GPIO_SW_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID

#define XGPIO_IR_MASK		0x3 /**< Mask of all bits */


/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define         TASK_STK_SIZE    		8192             // Size of each task's stacks (# of WORDs)

#define         TASK1_PRIO    	14               // Application tasks priorities
#define         TASK2_PRIO    	15
#define         TASK3_PRIO    	16

#define EVENT1  0x01
#define EVENT2  0x02
#define EVENT3  0x04

#define Task1_RDY  0x01
#define Task2_RDY  0x02
#define Task3_RDY  0x04
#define All_Tasks  0x07


/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/


CPU_STK 		StartupTaskStk[UCOS_START_TASK_STACK_SIZE];
OS_TCB 			StartupTaskTCB;

CPU_STK         Task1Stk[TASK_STK_SIZE];
OS_TCB			Task1TCB;

CPU_STK         Task2Stk[TASK_STK_SIZE];
OS_TCB			Task2TCB;

CPU_STK         Task3Stk[TASK_STK_SIZE];
OS_TCB			Task3TCB;

OS_FLAG_GRP FlagGroup1;
OS_FLAG_GRP FlagGroup2;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void            Task1(void *data);						// Function prototypes of tasks
void            Task2(void *data);						// Function prototypes of tasks
void            Task3(void *data);						// Function prototypes of tasks

void 			StartupTask (void *p_arg);

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


    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSFlagCreate(&FlagGroup1, "Flag Group1", (OS_FLAGS)0, &err);
	OSFlagCreate(&FlagGroup2, "Flag Group2", (OS_FLAGS)0, &err);

    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               TASKS
*********************************************************************************************************
*/

void Task1(void* data)
{
	OS_ERR err;
	CPU_TS ts;
	OS_FLAGS Flags;

	while(1)
	{
		OSFlagPend(&FlagGroup2, Task1_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		xil_printf("Debut tache 1\n");
		xil_printf("Attente sur event1 et sur event2\n");
		OSFlagPend(&FlagGroup1, EVENT1 + EVENT2, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_FLAG_CONSUME, &ts, &err);
		xil_printf("De T1, effacement des bits \n"); //OS_FLAG_CONSUME efface les flags.
		xil_printf("valeur du flag : %d\n", FlagGroup1.Flags);

		xil_printf("Tache 1 s'endort\n");
		OSTimeDlyHMSM(0,0,10,0, OS_OPT_TIME_DLY, &err);
	}
}

void Task2(void* data)
{
	OS_ERR err;
	CPU_TS ts;
	OS_FLAGS Flags;

	while(1)
	{
		OSFlagPend(&FlagGroup2, Task2_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		xil_printf("Debut tache 2\n");
		xil_printf("Attente sur event2 et sur event3\n");
		OSFlagPend(&FlagGroup1, EVENT2 + EVENT3, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_FLAG_CONSUME, &ts, &err);
		xil_printf("De T2, effacement des bits \n");
		xil_printf("valeur du flag : %d\n", FlagGroup1.Flags);

		xil_printf("Tache 2 s'endort\n");
		OSTimeDlyHMSM(0,0,10,0, OS_OPT_TIME_DLY, &err);
	}
}

void Task3(void* data)
{
	OS_ERR err;
	CPU_TS ts;


	while(1)
	{
		OSFlagPend(&FlagGroup2, Task3_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		xil_printf("Debut tache 3\n");
		xil_printf("Event1 generer\n");
		OSFlagPost(&FlagGroup1, EVENT1, OS_OPT_POST_FLAG_SET, &err);
		xil_printf("Event2 generer\n");
		OSFlagPost(&FlagGroup1, EVENT2, OS_OPT_POST_FLAG_SET, &err);
		xil_printf("Event3 generer\n");
		OSFlagPost(&FlagGroup1, EVENT3, OS_OPT_POST_FLAG_SET, &err);
		xil_printf("Tache 3 se termine\n");
		OSTimeDlyHMSM(0,0,5,0, OS_OPT_TIME_DLY, &err);
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
	CPU_TS ts;

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

    UCOS_Print("Programme initialise - \r\n");

    OSTaskCreate(&Task1TCB,"Task1", Task1, (void *) 0, TASK1_PRIO,
       &Task1Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

    OSTaskCreate(&Task2TCB,"Task2", Task2, (void *) 0, TASK2_PRIO,
       &Task2Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

    OSTaskCreate(&Task3TCB,"Task3", Task3, (void *) 0, TASK3_PRIO,
       &Task3Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

    OSFlagPost(&FlagGroup2, All_Tasks , OS_OPT_POST_FLAG_SET, &err);

    OSTimeDlyHMSM(0,0,50,0, OS_OPT_TIME_DLY, &err);     // On laisse ex�cuter 50 sec.

    UCOS_Print("Prepare to shutdown System - \r\n");

    OSFlagPost(&FlagGroup2, All_Tasks, OS_OPT_POST_FLAG_CLR, &err);

    OSTaskSuspend((OS_TCB *)0,&err);                 // On pourrait aussi faire delete des taches

}

