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

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define				TASK_STK_SIZE			8192                // Size of each task's stacks (# of WORDs)
#define				N						256					// Size of the home made message queue
#define          	MSG_QUEUE_SIZE			256                 // Size of uC/OS-III message queue

#define          	TaskStatPrio     		19                	// Application tasks priorities
#define          	TaskCons1Prio     		23
#define          	TaskCons2Prio    		23
#define          	TaskProdFifoPrio  		22
#define          	TaskDrivFifoPrio 	    21
#define          	TaskDevFifoPrio   		20

// Evenements lies aux taches - ici on personalise le depart de sorte qu'on peut stopper 1 seule tache si besoin est
#define TaskStat_RDY  		0x01
#define TaskCons1_RDY  		0x02
#define TaskCons2_RDY  		0x04
#define TaskProdFifo_RDY  	0x08
#define TaskConsFifo_RDY  	0x10
#define TaskDevFifo_RDY  	0x20
// Mask for all
#define ALL_TASKS			0x3F     // Permet de demarrer ou stopper toutes les taches au meme moment

#define 				New_Sample_Event  		0x02

static CPU_STK          TaskStatStk[TASK_STK_SIZE];
static CPU_STK          TaskCons1Stk[TASK_STK_SIZE];
static CPU_STK          TaskCons2Stk[TASK_STK_SIZE];
static CPU_STK          TaskProdFifoStk[TASK_STK_SIZE];
static CPU_STK          TaskDrivFifoStk[TASK_STK_SIZE];
static CPU_STK          TaskDevFifoStk[TASK_STK_SIZE];
static CPU_STK 		 	StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

static OS_TCB           TaskStatTCB;
static OS_TCB			TaskCons1TCB;
static OS_TCB           TaskCons2TCB;
static OS_TCB           TaskProdFifoTCB;
static OS_TCB           TaskDrivFifoTCB;
static OS_TCB           TaskDevFifoTCB;
static OS_TCB 			StartupTaskTCB;

OS_FLAG_GRP 		 	FlagGroup;


typedef struct {
	unsigned short value;
	unsigned char device;
} Msg;

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/


/* This disables all code until the next "#endif" */

OS_Q        	Fifo;                            // Message queue pointer

OS_MUTEX		Mutex;
OS_SEM			NbItems;
OS_SEM			NbSlots;
OS_SEM			Irq1;

OS_MUTEX 		mutPrint;

Msg buffer[N];
int add = 0, rem = 0;
unsigned short reg;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void             	TaskStat(void *data);               // Function prototypes of tasks
void             	TaskCons1(void *data);
void             	TaskCons2(void *data);
void	 		 		TaskProdFifo(void *data);
void             	TaskDrivFifo(void *data);
void             	TaskDevFifo(void *data);
void 				StartupTask (void *p_arg);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

int main (void)
{
    OS_ERR os_err;

    UCOS_LowLevelInit();

    CPU_Init();
    Mem_Init();
    OSInit(&os_err);

	OSTaskCreate(&TaskStatTCB, "TaskStat", TaskStat, (void*)0, TaskStatPrio, &TaskStatStk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 1, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	OSTaskCreate(&TaskCons1TCB, "TaskCons1", TaskCons1, (void*)0, TaskCons1Prio, &TaskCons1Stk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 1, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	OSTaskCreate(&TaskCons2TCB, "TaskCons2", TaskCons2, (void*)0, TaskCons2Prio, &TaskCons2Stk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	OSTaskCreate(&TaskProdFifoTCB, "TaskProdFifo", TaskProdFifo, (void*)0, TaskProdFifoPrio, &TaskProdFifoStk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	OSTaskCreate(&TaskDrivFifoTCB, "TaskDrivFifo", TaskDrivFifo, (void*)0, TaskDrivFifoPrio, &TaskDrivFifoStk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
	OSTaskCreate(&TaskDevFifoTCB, "TaskDevFifo", TaskDevFifo, (void*)0, TaskDevFifoPrio, &TaskDevFifoStk[0u],
		TASK_STK_SIZE / 2, TASK_STK_SIZE, 1, 0, (void*)0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);
    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &os_err);

	OSMutexCreate(&Mutex, "Mutex", &os_err);
	OSSemCreate(&NbItems, "NbItems", 0, &os_err);       // Nombre de places occupes, au depart 0
	OSSemCreate(&NbSlots, "NbSlots", N, &os_err);	    // Nombre de places disponibles, au d�part N
	OSSemCreate(&Irq1, "Irq1", 0, &os_err);
	OSQCreate(&Fifo, "Fifo", MSG_QUEUE_SIZE, &os_err); // Create a message queue

	OSMutexCreate(&mutPrint, "mutPrint", &os_err);

	OSFlagCreate(&FlagGroup, "Flag Group", (OS_FLAGS)0, &os_err);

    OSStart(&os_err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                              TASKS
*********************************************************************************************************
*/

void  TaskStat (void *data)
{
	OS_ERR err;
	CPU_TS ts;

	int i=0;


	while (1) {

		OSFlagPend(&FlagGroup, TaskStat_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_DLY, &err);                      // Wait 1 second
		UCOS_Printf("******** Statistics ******** \n");
		xil_printf("Nb de paquets actuel dans Fifo : %d \n", Fifo.MsgQ.NbrEntries);
		xil_printf("Nb de paquets maximum dans Fifo : %d \n", Fifo.MsgQ.NbrEntriesMax);

   }
}


  /* This disables all code until the next "#endif" */
void  TaskCons1 (void *pdata)
{
	OS_ERR err;
	CPU_TS ts;
	Msg out;
	pdata = pdata;
	while (1) {
		OSFlagPend(&FlagGroup, TaskCons1_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		OSSemPend(&NbItems, 0, OS_OPT_PEND_BLOCKING, &ts, &err);	           // Acquire semaphore
		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
		out = buffer[rem++];		//memcpy
		if (rem >= N)
			rem = 0;
		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);                     // Release semaphore
		OSSemPost(&NbSlots, OS_OPT_POST_1, &err);
		UCOS_Printf("Cons1 Value = %d Device:  %d\n", out.value, out.device);
	}
}

void  TaskCons2 (void *pdata)
{
	OS_ERR err;
	CPU_TS ts;
	Msg out;
   	pdata = pdata;
	while(1){
		OSFlagPend(&FlagGroup, TaskCons2_RDY, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		OSSemPend(&NbItems, 0, OS_OPT_PEND_BLOCKING, &ts, &err);	           // Acquire semaphore
		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
 			out = buffer[rem++];		//memcpy
			if(rem>=N)
				rem =0;
		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);                     // Release semaphore
		OSSemPost(&NbSlots, OS_OPT_POST_1, &err);
		UCOS_Printf("Cons2 Value = %d Device:  %d\n",out.value, out.device);
	}
}

void  TaskProdFifo (void *data)
{
	int WAITFORComputing;
	OS_ERR err;
	CPU_TS ts;
	OS_TICK actualticks = 0;
	OS_MSG_SIZE msg_size;
	Msg *msg=NULL;
	data = data;

	while(1){
		OSFlagPend(&FlagGroup, TaskProdFifo_RDY , 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		msg = OSQPend(&Fifo, 0, OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);   // Wait forever for message

		WAITFORComputing = (rand() %3);
		/* On simule un temps de traitement */
		actualticks = OSTimeGet(&err);
		while (WAITFORComputing + actualticks > OSTimeGet(&err));

		xil_printf("\nProdFifo Value = %d Device:  %d\n",msg->value, msg->device);

		OSSemPend(&NbSlots, 0, OS_OPT_PEND_BLOCKING, &ts, &err);		// Acquire semaphore
		OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			buffer[add++] = *msg;	//memcpy
			if(add>=N)
				add =0;
   		OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);                  //Release semaphore
		OSSemPost(&NbItems, OS_OPT_POST_1, &err);
		free(msg);
	}
}


void  TaskDrivFifo (void *data)
{

	OS_ERR err;
	CPU_TS ts;
	data = data;
	while (1) {
		OSFlagPend(&FlagGroup, TaskConsFifo_RDY , 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		OSFlagPend(&FlagGroup, New_Sample_Event, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING + OS_OPT_PEND_FLAG_CONSUME, &ts, &err);
		Msg *msg = malloc(sizeof(msg));
		msg->value = reg;
		msg->device = 1;
		OSQPost(&Fifo, msg, sizeof(msg), OS_OPT_POST_FIFO, &err);
		xil_printf("\nValeur produite: %d\n", reg);
		if (err == OS_ERR_Q_MAX || err == OS_ERR_MSG_POOL_EMPTY) {
			xil_printf("\nQueue pleine !\n");
			}
	}
}


void  TaskDevFifo (void *data)
// En supossant qu'on roule � 1000Hz, on �mule une interruption mat�rielle � toutes les ms,
// avec un repos de 800ms apr�s chaque rafale de 255ms
{
	 int i;
	unsigned short value = 0;
	OS_ERR err;
	CPU_TS ts;

	data = data;
	while(1){
		OSFlagPend(&FlagGroup, TaskDevFifo_RDY , 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);
		xil_printf("\Debut du stream !\n");
 		for(i=0; i<255; i++) {
 	        OSTimeDly(1, OS_OPT_TIME_PERIODIC, &err);
			reg = value++;
			OSFlagPost(&FlagGroup, New_Sample_Event, OS_OPT_POST_FLAG_SET, &err);
		}
 		xil_printf("\nFin du stream !\n");
 		OSTimeDlyHMSM(0, 0, 0, 800, OS_OPT_TIME_HMSM_STRICT, &err);     	  // Delay for 800 millisecond
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
    UCOS_Printf("Frequence courante du tick d horloge - %d\r\n", OS_CFG_TICK_RATE_HZ);

    OSFlagPost(&FlagGroup, ALL_TASKS , OS_OPT_POST_FLAG_SET, &err);

    OSTimeDlyHMSM(0,0,10,0, OS_OPT_TIME_DLY, &err);			// On laisse executer 10 sec.

    UCOS_Print("Prepare to shutdown System - \r\n");

    OSFlagPost(&FlagGroup, ALL_TASKS, OS_OPT_POST_FLAG_CLR, &err);

    OSTaskSuspend((OS_TCB *)0,&err);      // On pourrait aussi faire delete des taches

}





