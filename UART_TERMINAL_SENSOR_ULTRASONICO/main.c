#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "inc/tm4c123gh6pm.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_timer.h"
#include "inc/hw_gpio.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"

char TX[2];
char RX[5];
char ARRAY_PENT_DISTANCIA[3];
char ARRAY_PDEC_DISTANCIA[1];
volatile double DISTANCIA = 0;
volatile double DISTANCIA_PROMEDIO = 0;
volatile int PENT_DISTANCIA = 0;
volatile double PDEC_DISTANCIA = 0;
volatile uint32_t DURACION_ECHO_CICLOS = 0;
volatile bool ACTIVAR_TRIG = true;
uint32_t ui32Status;
uint32_t iter = 0;

// FUNCI�N PARA ESCRIBIR TEXTOS SOBRE LA TERMINAL
void TerminalText(char * array) {
  while ( * array) {
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);
    UARTCharPut(UART0_BASE, * array);
    array++;
  }
  UARTCharPut(UART0_BASE, '\r');
  UARTCharPut(UART0_BASE, '\n');
}

// MANEJADOR DE LA INTERRUPCI�N DEL TMR0 -- CONTADOR PARA 10us
void Timer0IntHandler() {
  TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
  TimerDisable(TIMER0_BASE, TIMER_A);
  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);

}

// MANEJADOR DE LA INTERRUPCI�N DEL PUERTO B -- SE�AL ECHO
void GPIOIntHandler(void) {

  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);
  ACTIVAR_TRIG = false;
  GPIOIntClear(GPIO_PORTB_BASE, GPIO_INT_PIN_1);

  if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_1) == GPIO_PIN_1) {
    HWREG(TIMER2_BASE + TIMER_O_TAV) = 0;
    TimerEnable(TIMER2_BASE, TIMER_A);
  } else {
    TimerDisable(TIMER2_BASE, TIMER_A);
// LIMITE PARA 2 cm - 400cm
    if ((DURACION_ECHO_CICLOS >= 9329) && (DURACION_ECHO_CICLOS <= 1865889)) {

      DISTANCIA = DURACION_ECHO_CICLOS / 16;
      DISTANCIA = DISTANCIA / 10;
      DISTANCIA = DISTANCIA / 10;
      DISTANCIA = DISTANCIA / 10;
      DISTANCIA = DISTANCIA / 10;
      DISTANCIA = DISTANCIA / 10;
      DISTANCIA = DISTANCIA * 343;

// PROMEDIO DE 25 MEDICIONES
      if (iter <= 24) {
        DISTANCIA_PROMEDIO = DISTANCIA_PROMEDIO + DISTANCIA;
        iter++;
      }

      if (iter > 24) {
        iter = 0;
        DISTANCIA_PROMEDIO = DISTANCIA_PROMEDIO / 25;

// CONDICIONES DE VENTANA
        if  ((DISTANCIA_PROMEDIO >= 5) && (DISTANCIA_PROMEDIO <= 25)){
            TerminalText("ROJO");
        }
        else if ((DISTANCIA_PROMEDIO >= 26) && (DISTANCIA_PROMEDIO <= 50)){
            TerminalText("BUZZER");
        }
        else if ((DISTANCIA_PROMEDIO >= 51) && (DISTANCIA_PROMEDIO <= 100)){
            TerminalText("VERDE");
        }
// PARTE ENTERA Y PARTE DECIMAL
        PENT_DISTANCIA = (int) DISTANCIA_PROMEDIO;
        PDEC_DISTANCIA = (DISTANCIA_PROMEDIO - PENT_DISTANCIA) * 10;

// CONVERSI�N A CADENA DE CARACTERES
        ltoa(PENT_DISTANCIA, ARRAY_PENT_DISTANCIA, 10);
        ltoa(PDEC_DISTANCIA, ARRAY_PDEC_DISTANCIA, 10);

// PSEUDO CONCATENACI�N PARA MOSTRAR EN LA TERMINAL
        UARTCharPut(UART0_BASE, ARRAY_PENT_DISTANCIA[0]);
        UARTCharPut(UART0_BASE, ARRAY_PENT_DISTANCIA[1]);
        UARTCharPut(UART0_BASE, ARRAY_PENT_DISTANCIA[2]);
        UARTCharPut(UART0_BASE, '.');
        UARTCharPut(UART0_BASE, ARRAY_PDEC_DISTANCIA[0]);
        UARTCharPut(UART0_BASE, '\r');
        UARTCharPut(UART0_BASE, '\n');
        DISTANCIA_PROMEDIO = 0;
      }
    }
    ACTIVAR_TRIG = true;
  }
}

// MANEJADOR TX DEL MODULO UART0 -- ACTIVA O DESACTIVA LAS MEDICIONES
void UARTIntHandler(void) {
  ui32Status = UARTIntStatus(UART0_BASE, true);
  UARTIntClear(UART0_BASE, ui32Status);
  while (UARTCharsAvail(UART0_BASE)) {
    RX[0] = UARTCharGet(UART0_BASE);
    RX[1] = UARTCharGetNonBlocking(UART0_BASE);
    UARTCharPut(UART0_BASE, RX[0]);
    if (RX[0]== '1'){
        ACTIVAR_TRIG = true;
    }
    if (RX[0]== '2'){
        ACTIVAR_TRIG = false;
    }
    UARTCharPut(UART0_BASE, '\r');
    UARTCharPut(UART0_BASE, '\n');

  }
}
int main(void) {
// OSCILADOR 80Mhz
  SysCtlClockSet(SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_SYSDIV_2_5);
// CONFIGURACI�N DEL LA INTERRUPCION PARA DETECTAR EL ECHO
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
  GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_1);
  GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
  GPIOIntEnable(GPIO_PORTB_BASE, GPIO_INT_PIN_1);
  GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_BOTH_EDGES);
  IntPrioritySet(INT_GPIOB, 0);
  IntRegister(INT_GPIOB, GPIOIntHandler);
// CONFIGURACI� DEL PIN PARA EL PULSO TRIGER
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_0);
// CONFIGURACI�N DEL UART0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE));
  IntEnable(INT_UART0);
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
  UARTEnable(UART0_BASE);
// CONFIGURACI�N DEL TMR0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
  TimerConfigure(TIMER0_BASE, TIMER_CFG_ONE_SHOT_UP);
  TimerLoadSet(TIMER0_BASE, TIMER_A, 8);
  TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
  TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler);
  IntEnable(INT_TIMER0A);
// CONFIGURACI�N DEL TMR2
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
  TimerConfigure(TIMER2_BASE, TIMER_CFG_ONE_SHOT_UP);

  IntMasterEnable();
  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);



  while (1) {

    if (ACTIVAR_TRIG) {
      GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_PIN_0);
      TimerEnable(TIMER0_BASE, TIMER_A);
      IntEnable(INT_TIMER0A);
      IntEnable(INT_GPIOB);
    }

  }
}
