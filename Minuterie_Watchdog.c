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

#define         TASK_Periodic_PRIO    	15               // Application tasks priorities
//#define			WAITFOR500TICKS			500

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/


CPU_STK 		StartupTaskStk[UCOS_START_TASK_STACK_SIZE];
OS_TCB 			StartupTaskTCB;

CPU_STK          TaskPeriodicStk[TASK_STK_SIZE];     		// Startup    task stack
OS_TCB			 TaskPeriodicTCB;

OS_SEM			 Synchro;

OS_TMR      	 Scheduler;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void            TaskPeriodic(void *data);						// Function prototypes of tasks
void			SchedulerFct(OS_TMR  *p_tmr, void    *p_arg);

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

	OSSemCreate(&Synchro, "Synchro", 0, &err);


	OSTmrCreate(&Scheduler,         		/* p_tmr          */
                   "Scheduler",           	/* p_name         */
                    0,                    	/* dly            */
                    10,                     /* period         */
//					OS_OPT_TMR_ONE_SHOT,   	/* opt            */
					OS_OPT_TMR_PERIODIC,   	/* opt            */
                    SchedulerFct,         	/* p_callback     */
                    0,                     	/* p_callback_arg */
                   &err);                  	/* p_err          */


    OSTaskCreate(&TaskPeriodicTCB,"TaskPeriodic", TaskPeriodic, (void *) 0, TASK_Periodic_PRIO,
       &TaskPeriodicStk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);



    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               WATCHDOG TASK
*********************************************************************************************************
*/

void  TaskPeriodic (void *data)
{
    OS_ERR  err;
	CPU_TS ts;
	OS_TICK actualticks = 0;
	int WAITFORTICKS;




	OSTmrStart(&Scheduler, &err);        // D�marrage ici si on est en mode p�riodique

	while(1)
	{

		//OSTmrStart(&Scheduler, &err);        // D�marrage ici si on en mode one shot

		// Le code applicatif serait ici. Pour tester j'ai mis une attente active de 500 ticks
		// mais ce delai pourrait changer d une fois a l autre en autant qu il est inferier a 1000



		xil_printf("Attente active de maximum 500 ticks - on est au tick no %d \n", OSTimeGet(&err));

		do { WAITFORTICKS = (rand() %500); } while (WAITFORTICKS == 0);
		actualticks = OSTimeGet(&err);
		while(WAITFORTICKS + actualticks > OSTimeGet(&err));

		xil_printf("Fin de l execution  - Tick no %d \n", OSTimeGet(&err));
	    // Fin du code applicatif

		OSSemPend(&Synchro,0, OS_OPT_PEND_BLOCKING, &ts, &err);
		xil_printf("Fin de la periode  - Tick no %d \n", OSTimeGet(&err));

	}
}

void  SchedulerFct (OS_TMR  *p_tmr,
                     void    *p_arg)
{
  OS_ERR  err;
  OSSemPost(&Synchro, OS_OPT_POST_1, &err);
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

	xil_printf("Frequence du systeme: %d\n", OS_CFG_TICK_RATE_HZ);
	xil_printf("Frequence du watchdog: %d\n", OS_CFG_TMR_TASK_RATE_HZ);

    OSTaskSuspend((OS_TCB *)0,&err);

}

