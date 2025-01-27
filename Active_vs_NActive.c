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
* File: AttenteActiveVsNonActive.c
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

#define         TASK_STK_SIZE    8192                // Size of each task's stacks (# of WORDs)

#define TASKACTIVE_PRIO		  15   // Priorit� de TaskActive
#define TASKNONACTIVE_PRIO    14   // Priorit� de TaskNonActive

#define WAITFOR1000TICKS 	  1000

#define 	All_Tasks  			0x11
#define 	TaskActiveMask  	0x01
#define 	TaskNonActiveMask   0x10

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/


CPU_STK           TaskActiveStk[TASK_STK_SIZE];
CPU_STK           TaskNonActiveStk[TASK_STK_SIZE];
CPU_STK 		  StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

OS_TCB			  TaskActiveTCB;
OS_TCB			  TaskNonActiveTCB;
OS_TCB 			  StartupTaskTCB;

OS_FLAG_GRP 		  FlagGroup;

/*
 *********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void    TaskActive(void *data);
void    TaskNonActive(void *data);
void 	StartupTask (void *p_arg);

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

	OSTaskCreate(&TaskActiveTCB,"TaskActive", TaskActive, (void *) 0, TASKACTIVE_PRIO,
       &TaskActiveStk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&TaskNonActiveTCB,"TaskNonActive", TaskNonActive, (void *) 0, TASKNONACTIVE_PRIO,
       &TaskNonActiveStk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSFlagCreate(&FlagGroup, "Flag Group", (OS_FLAGS)0, &err);

    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               TASKS
*********************************************************************************************************
*/

void TaskActive(void* data)
{
	OS_ERR err;
	OS_TICK actualticks = 0;
	CPU_TS ts;

	while(1)
	{

		OSFlagPend(&FlagGroup, TaskActiveMask, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

		actualticks = OSTimeGet(&err);

		xil_printf("Debut attente active de 1000 ticks - OSTickCtr: %d\r\n", OSTimeGet(&err));
		while(WAITFOR1000TICKS + actualticks > OSTimeGet(&err));

		xil_printf("Fin tache active - OSTickCtr: %d\r\n", OSTimeGet(&err));
		OSTimeDlyHMSM(0,0, 10,0, OS_OPT_TIME_HMSM_STRICT, &err);
	}
}

void TaskNonActive(void* data)
{
	OS_ERR err;
	CPU_TS ts;

	while(1)
	{

		OSFlagPend(&FlagGroup, TaskNonActiveMask, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

		xil_printf("Debut attente non active de 1000 ticks - OSTickCtr: %d\r\n", OSTimeGet(&err));
		OSTimeDly(1000,OS_OPT_TIME_DLY, &err);
//		OSTimeDly(1000,OS_OPT_TIME_PERIODIC, &err);        // On verra dans le bloc 5 la difference entre OS_OPT_TIME_DLY et OS_OPT_TIME_PERIODIC

		xil_printf("Fin tache non active - OSTickCtr: %d\r\n", OSTimeGet(&err));
		OSTimeDlyHMSM(0,0,10,0, OS_OPT_TIME_HMSM_STRICT, &err);

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

    UCOS_Print("Programme initialise - \r\n");
    UCOS_Printf("Frequence courante du tick d horloge - %d\r\n", tick_rate);

    OSFlagPost(&FlagGroup, All_Tasks , OS_OPT_POST_FLAG_SET, &err);

    OSTimeDlyHMSM(0,0,40,0, OS_OPT_TIME_DLY, &err);

    UCOS_Print("Prepare to shutdown System - \r\n");

    OSFlagPost(&FlagGroup, All_Tasks, OS_OPT_POST_FLAG_CLR, &err);

    OSTaskSuspend((OS_TCB *)0,&err);  // On pourrait aussi faire delete des taches

}

