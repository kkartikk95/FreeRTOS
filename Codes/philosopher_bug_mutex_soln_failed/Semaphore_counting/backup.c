/*

Replaced contenets in lpc21xx with that of lpc214x to remove warnings 
Include startup.s from experiments in case of data abort
Counting.axf: Error: L6218E: Undefined symbol pvPortMalloc (referred from tasks.o). error is caused when heap 2 is not included
*/

/************************************************************************ 
Written by: K V S SUMAKAR and KARTIKEYAN V, ERTS Lab, IIT Bombay. Date: 23 June 2016
 IDE: Keil uVision4


Problem:If two different tasks want to control the motion It would create a problem,my using Semaphore as MUTEX we
		 can restict the usage of Motion function by other pins.

		 Here two forward task tries to move the bot forward for few millisecs and back function tries to move it back for few msec's
		 now while forward task uses Motors for forward motion Back function is denied acess for motion control 
		 and vice versa.

		 The Provision or denying of acsess is displayed on the serial Monitor.

Concepts covered:	
Mutex semaphores definition, creation and usage.


 Note:
 
 1. Do don't use the default Startup.s	provided by Keil. 
 	Instead follow the procedure given below :
	a. Copy the Startup.S file from any example project into the folder containing the project.
	b. Right click on the group icon in the project window and click on 'Add files to Group <group name>'
	c. Select and add the Startup.s file which was previously added.

 2. Make sure that in the Target options following settings are 
 	done for proper operation of the code

 	Device -> Microcontroller	: LPC2148
 	Target -> Frequency			: 12 Mhz
 	Output -> Create Hex File	: Checked (For more information read section 4.3.1 "Setting up Project in Keil uVision" in the hardware manual)
	C/C++  -> Include paths 	: ..\..\Source\include;..\..\Source\portable\RVDS\ARM7_LPC21xx;..\<project-folder-name>
	ASM    -> Include paths 	: ..\..\Source\portable\RVDS\ARM7_LPC21xx


 3. Ensure that following settings are done in Startup.s configuration wizard:

 	Clock Settings:
		
	PLL Steup	 >>	 MSEL=5, PSEL=2
	VPBDIV Setup >>  VPBCLK = CPU Clock
	
 4. Include the following FreeRTOS C files in the project either in the same group, or by creating a new group in the project.
	
	 ( These will be available in the FreeRTOS source files. )
	
	FILE		SOURCE (may change according to location of FreeRTOS source code)
	list.c	 	FreeRTOSv8.0.1\FreeRTOS\Source\list.c
	queue.c		FreeRTOSv8.0.1\FreeRTOS\Source\queue.c
	tasks.c		FreeRTOSv8.0.1\FreeRTOS\Source\tasks.c
	heap_2.c	FreeRTOSv8.0.1\FreeRTOS\Source\portable\MemMang\heap_2.c
	port.c		FreeRTOSv8.0.1\FreeRTOS\Source\portable\RVDS\ARM7_LPC21xx\port.c
	portASM.s	FreeRTOSv8.0.1\FreeRTOS\Source\portable\RVDS\ARM7_LPC21xx\portASM.s
	
 5. Copy FreeRTOSConfig.h file into	the project folder. Configure it according to the application. 
 	use this link for reference : http://www.freertos.org/a00110.html
 
	 set configUSE_COUNTING_SEMAPHORES to 1	 to use counting semaphore
	For more details refer section 4.8 in the hardware manual.




Declaration of Mutex 
xSemaphoreTake( SemaphoreHandle_t xSemaphore,TickType_t xTicksToWait );

	xTicksToWait:Is the time for which the function will wait for resource to be available.
	xSemaphore : Showes if resource is available or not,Binary in case of Mutex
*/
//****************************************************************************************************/
/* 
Note: To use mutex semaphore you need to initialize  configUSE_MUTEXES to 1 
*/

#include<stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "LCD.h"
#include"semphr.h"

SemaphoreHandle_t xSemaphore=0,xMutex=0;//Creation of Variable for semaphore

unsigned char Temp=0; 
int forks_avail[5]={0,0,0,0,0}; 
#define Fosc            12000000                    //10MHz~25MHz
#define Fcclk           (Fosc * 5)                  //Fosc(1~32)<=60MHZ
#define Fcco            (Fcclk * 4)                 //CCO Fcclk 2?4?8?16?156MHz~320MHz
#define Fpclk           (Fcclk / 4) * 1             //VPB(Fcclk / 4) 1?2?4
#define  UART_BPS	9600 		//Change Baud Rate Setting here
int i=0,j=0;
void DelaymSec(unsigned int j);


void Init_Peripherals(void);
void Init_Ports(void);
void  __irq IRQ_UART0(void);

void Init_UART0(void);
void UART0_SendByte(unsigned char data);	//For sending a byte of Data through serial monitor
void UART0_SendStr(const unsigned char *str);//For sending a string of Data through serial monitor


//This function is UART0 Receive ISR. This functions is called whenever UART0 receives any data
void  __irq IRQ_UART0(void)
{  
 Temp = U0RBR;			
 
 VICVectAddr = 0x00;
 UART0_SendByte(Temp);	//Echo Back received character
}		


/************************************************************

	Function 		: Init_UART0
	Return type		: None
	Parameters		: None
	Description 	: Initialises UART0 module. 
************************************************************/
void Init_UART0(void)
{  
   unsigned int Baud16;
   PINSEL0&=0xFFFFFFF0;
   PINSEL0|=0x00000005;

   U0LCR = 0x83;		            // DLAB = 1
   Baud16 = (Fpclk / 16) / UART_BPS;  
   U0DLM = Baud16 / 256;							
   U0DLL = Baud16 % 256;						
   U0LCR = 0x03;
   U0IER = 0x00000001;		//Enable Rx interrupts

   VICIntSelect = 0x00000000;		// IRQ
   VICVectCntl0 = 0x20|6;			// UART0
   VICVectAddr0 = (int)IRQ_UART0; 	//UART0 Vector Address
   VICIntEnable = (1<<6);	// Enable UART0 Rx interrupt

}
				

//This function sends a single character on the serial port
void UART0_SendByte(unsigned char data)
{  
   U0THR = data;				    
   while( (U0LSR&0x40)==0 );	    
}

//This function sends a string of characters on the serial port
void UART0_SendStr(const unsigned char *str)
{  
   while(1)
   {  
      if( *str == '\0' ) break;
      UART0_SendByte(*str++);	    
   }
}

void Init_Ports(void)
{Init_LCD_Pin();
 }

void Init_Peripherals(void)
{
 Init_Ports();
 Init_UART0();
}

void DelaymSec(unsigned int j)		  //App 1mSec delay
{  
 unsigned int  i;
 for(;j>0;j--)
 {
  for(i=0; i<10000; i++);
 } 
}
	void vfork1( void * pvParameters )
			{	while(1)
				{  
				i=0;
				  
				  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
				//	UART0_SendStr("P1:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P1:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr("P1:Ate \n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						   // UART0_SendStr("P1:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ 
								UART0_SendStr("P1:Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   {
				   	UART0_SendStr("P1:Hungry and Thinking \n");
					vTaskDelay(3000);
					}
				}
				 }
			   void vfork2( void * pvParameters )
			{	
				while(1)
				{
				 i=1;
				  
				  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
				//	UART0_SendStr("P2:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P2:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr("P2:Ate\n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						 //   UART0_SendStr("P2:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ 
							    UART0_SendStr("P2:Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   {
				   	UART0_SendStr("P2:Hungry and Thinking \n");
					vTaskDelay(3000);
					}
				}
				}
			void vfork3( void * pvParameters )
			{	 
			 while(1)
				{
			 i=2;
				  
				  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
					//UART0_SendStr("P3:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P3:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr("P3:Ate  \n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						  //  UART0_SendStr("P3:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ 
								UART0_SendStr("P3:Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   {
				   	UART0_SendStr("P3:Hung & Think \n");
					vTaskDelay(3000);
					}
				}
				}
			void vfork4( void * pvParameters )
			{	 
				while(1)
				{
				 i=3;
				  
				  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
				//	UART0_SendStr("P4:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P4:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr("P4:Ate \n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						  //  UART0_SendStr("P4:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ 
								UART0_SendStr("P4:Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   {
				   	UART0_SendStr("P4:Hungry and Thinking \n");
					vTaskDelay(3000);
					}
				}
				}
			void vfork5( void * pvParameters )
			{	 
				while(1)
				{
				 i=4;
				  
				  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
				//	UART0_SendStr("P5:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P5:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr("P5:Ate \n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						    //UART0_SendStr("P5:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ 
								UART0_SendStr("P5:Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   {
				   	UART0_SendStr("P5:Hungry and Thinking \n");
					vTaskDelay(3000);
					}
				}
				 }
			
	void vfork( void * pvParameters )
			{const unsigned char* str;
			str = ( const unsigned char * ) pvParameters; 
				UART0_SendStr(&str[0]);
				
				if(str[1]=='1')
				{i=1;}
				if(str[1]=='2')
				{i=2;}
				if(str[1]=='3')
				{i=3;}
				if(str[1]=='4')
				{i=4;}
				if(str[1]=='5')
				{i=5;} 
				while(1)
				{  if(( xSemaphoreTake( xSemaphore, 1000 ) == pdTRUE )&&(forks_avail[i]==0))
				  	{
					forks_avail[i]=1;
				//	UART0_SendStr("P5:Right fork obtained,Waiting for Left fork\n");
					 	
						if(( xSemaphoreTake( xSemaphore, 2000 ) == pdTRUE )&&(forks_avail[(i+1)%5]==0))
				  				   {
								   
				  						 	forks_avail[(i+1)%5]=1;
										//	UART0_SendStr("P5:Left fork obtained Eating :)\n");
											vTaskDelay(2000);
											UART0_SendStr(&str[0]);
											UART0_SendStr(":Ate \n");
									   		xSemaphoreGive(xSemaphore);
									 		xSemaphoreGive(xSemaphore);
											forks_avail[i]=0;	 
										 	forks_avail[(i+1)%5]=0;
                   						    //UART0_SendStr("P5:Thinking \n");
											vTaskDelay(3000);

									}

						else
								{ UART0_SendStr(&str[0]);
								UART0_SendStr(":Returned Right fork:( \n");
	    					    xSemaphoreGive(xSemaphore);
								forks_avail[i]=0;
								}
				  
				  	}

				   else
				   { UART0_SendStr(&str[0]);
				   	UART0_SendStr(":Hungry and Thinking \n");
					vTaskDelay(3000);
					}
				}
				 }
			
			
		
			
	
  int main()
{	
 PINSEL0 = 0x00000000;		// Reset all pins as GPIO
 PINSEL1 = 0x00000000;
 PINSEL2 = 0x00000000;
 DelaymSec(40);
 	Init_Peripherals();
	
	
	 /*
	UART0_SendStr("\t\tCounting Semaphore\n");
	xSemaphore = xSemaphoreCreateCounting( 5, 5 );	//Use the Handle as a counting semaphore	  
	xMutex = xSemaphoreCreateMutex();
	  
	 if( xSemaphore != NULL )
    {	UART0_SendStr("\tSemaphore Created\n");
        
											//( void * ) 1
		xTaskCreate(vfork1,"Philospher 1", 300 ,NULL, tskIDLE_PRIORITY + 1, NULL);//Task Creation
    	xTaskCreate(vfork2,"Philospher 2", 300 ,NULL, tskIDLE_PRIORITY + 1, NULL);//Task Creation
		xTaskCreate(vfork3,"Philospher 3", 300 ,NULL, tskIDLE_PRIORITY + 1, NULL);//Task Creation
		xTaskCreate(vfork4,"Philospher 4", 300 ,NULL, tskIDLE_PRIORITY + 1, NULL);//Task Creation
		xTaskCreate(vfork5,"Philospher 5", 300 ,NULL, tskIDLE_PRIORITY + 1, NULL);//Task Creation
		
		
		xTaskCreate(vfork,"Philospher 1", 300 ,&j, tskIDLE_PRIORITY + 1, NULL);//Task Creation

		xTaskCreate(vfork,"Philospher 2", 300 ,&j, tskIDLE_PRIORITY + 1, NULL);//Task Creation

		xTaskCreate(vfork,"Philospher 3", 300 ,&j, tskIDLE_PRIORITY + 1, NULL);//Task Creation
		vTaskStartScheduler();  //Task Scheduling   					    
     }*/
xTaskCreate(vfork,"Philospher 3", 300 ,"P1", tskIDLE_PRIORITY + 1, NULL);//Task Creation
		vTaskStartScheduler();  //Task Scheduling   					    
		
	while(1)
	{UART0_SendStr("\t\tSemaphore not Created\n");}

}

