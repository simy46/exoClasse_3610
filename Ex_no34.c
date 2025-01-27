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
XGpio gpio_PressButton;

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



#define         TASK_A_PRIO    		10                // Application tasks priorities
#define			TASK_B_PRIO			8
#define 		WAITFOR10000TICKS 	10000

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

OS_PRIO My_Prio = 0;

CPU_STK         Task_A_Stk[TASK_STK_SIZE];     	// Startup    task stack
CPU_STK			Task_B_Stk[TASK_STK_SIZE];
CPU_STK 		StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

OS_TCB			Task_A_TCB;
OS_TCB			Task_B_TCB;
OS_TCB 			StartupTaskTCB;

  /* This disables all code until the next "#endif" */

OS_MUTEX		Mutex;
OS_SEM			Sem;			// le semaphore emule le reveil de la tache la plus prioritaire


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void            Task_A(void *data);               // Function prototypes of tasks
void			Task_B(void *data);
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
	OSSemCreate(&Sem, "Sem", 0, &err);

	OSTaskCreate(&Task_A_TCB,"Task_A", Task_A, (void *) 0, TASK_A_PRIO,
       &Task_A_Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

	OSTaskCreate(&Task_B_TCB,"Task_B", Task_B, (void *) 0, TASK_B_PRIO,
       &Task_B_Stk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               ISR
*********************************************************************************************************
*/

void gpio_isr(void *p_int_arg, CPU_INT32U source_cpu) {
		OS_ERR err;
		CPU_TS ts;
		int button_data = 0;

		button_data = XGpio_DiscreteRead(&gpio_PressButton, 1); //get button data

		XGpio_DiscreteWrite(&gpio_PressButton, 2, button_data); //write botton data to the LEDs

		XGpio_InterruptClear(&gpio_PressButton, XGPIO_IR_MASK);

		OSSemPend(&Sem, 0, OS_OPT_PEND_NON_BLOCKING, &ts, &err);
		UCOS_Print ("gpio isr \r\n");
		OSSemPost(&Sem,  OS_OPT_POST_1 + OS_OPT_POST_NO_SCHED, &err);
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

		OSSemPend(&Sem,0, OS_OPT_PEND_BLOCKING, &ts, &err);

		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);

		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);

	}
}

void Task_A(void *data)

{
	OS_ERR  err;
	CPU_TS ts;
	OS_TICK actualticks = 0;

	int cnt = 0;
	while(1)
	{

		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);

		PrintOwnerofMutexanditsPriority("TA avant attente active", &Mutex);

		UCOS_Print("Attente active de 10000 ticks \r\n");

		actualticks = OSTimeGet(&err);

		while(WAITFOR10000TICKS + actualticks > OSTimeGet(&err));

		PrintOwnerofMutexanditsPriority("TA apres attente active", &Mutex);

		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);

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

    initialize_gpio();

    initialize_axi_intc();

    connect_axi();

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

    UCOS_Print("Programme initialise - Pret a demarrer - Actionner bouton.\r\n");

    OSTaskSuspend((OS_TCB *)0,&err);

}

///////////////////////////////////////////////////////////////////////////
//						Interrupt Section
///////////////////////////////////////////////////////////////////////////

void initialize_gpio()
{
	if (XST_DEVICE_NOT_FOUND == XGpio_Initialize(&gpio_PressButton, GPIO_SW_DEVICE_ID))
		UCOS_Print("Erreur init gpio\n");
	XGpio_SetDataDirection(&gpio_PressButton, 1, 0xF);
	XGpio_SetDataDirection(&gpio_PressButton, 2, 0x0);
	XGpio_InterruptGlobalEnable(&gpio_PressButton);
	XGpio_InterruptEnable(&gpio_PressButton, XGPIO_IR_MASK);
}

int initialize_axi_intc() {
	int status;

	status = XIntc_Initialize(&axi_intc, XPAR_AXI_INTC_0_DEVICE_ID);
	if (status != XST_SUCCESS)
		return XST_FAILURE;

	return XST_SUCCESS;
}

/*int connect_fit_timer_1s_irq() {
	int status;

	status = XIntc_Connect(&axi_intc, FIT_1S_IRQ_ID, fit_timer_1s_isr, NULL);
		if (status != XST_SUCCESS)
			return status;

	XIntc_Enable(&axi_intc, FIT_1S_IRQ_ID);

	return XST_SUCCESS;
}

int connect_fit_timer_3s_irq() {
	int status;

	status = XIntc_Connect(&axi_intc, FIT_3S_IRQ_ID, fit_timer_3s_isr, NULL);
		if (status != XST_SUCCESS)
			return status;

	XIntc_Enable(&axi_intc, FIT_3S_IRQ_ID);

	return XST_SUCCESS;
}
*/


int connect_gpio_irq()			// mettre en commentaire en mode watchdog
{
	int status = XIntc_Connect(&axi_intc, GPIO_SW_IRQ_ID, gpio_isr, &gpio_PressButton);
	if (status == XST_SUCCESS)
		XIntc_Enable(&axi_intc, GPIO_SW_IRQ_ID);
	return status;
}

void connect_axi() {

	CPU_BOOLEAN succes = UCOS_IntVectSet (PL_INTC_IRQ_ID,
			                             1,
			                             0,
										 (UCOS_INT_FNCT_PTR)XIntc_DeviceInterruptHandler,
										 (void*)(uint32_t)axi_intc.CfgPtr->DeviceId);
	if (succes != DEF_OK)
		UCOS_Print ("connect axi : FAIL \n");
	succes = UCOS_IntSrcEn(PL_INTC_IRQ_ID);
	if (succes != DEF_OK)
		UCOS_Print ("enable axi : FAIL \n");

	connect_gpio_irq();						// mettre en commentaire en mode watchdog
//	connect_fit_timer_1s_irq();
//	connect_fit_timer_3s_irq();
	XIntc_Start(&axi_intc, XIN_REAL_MODE);


}

void cleanup() {
	/*
	 * Disconnect and disable the interrupt
	 */

	disconnect_intc_irq();
//	disconnect_fit_timer_1s_irq();
//	disconnect_fit_timer_3s_irq();
}

void disconnect_intc_irq() {
	UCOS_IntSrcDis(PL_INTC_IRQ_ID);
}

/*

 void disconnect_fit_timer_1s_irq() {

	XIntc_Disable(&axi_intc, FIT_1S_IRQ_ID);
	XIntc_Disconnect(&axi_intc, FIT_1S_IRQ_ID);

}

void disconnect_fit_timer_3s_irq() {

	XIntc_Disable(&axi_intc, FIT_3S_IRQ_ID);
	XIntc_Disconnect(&axi_intc, FIT_3S_IRQ_ID);

*/




