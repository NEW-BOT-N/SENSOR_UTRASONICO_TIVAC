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

// FUNCIÓN PARA ESCRIBIR TEXTOS SOBRE LA TERMINAL
void TerminalText(char * array) {
  while ( * array) {
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);
    UARTCharPut(UART3_BASE, * array);
    array++;
  }
  UARTCharPut(UART3_BASE, '\r');
  UARTCharPut(UART3_BASE, '\n');
}

// MANEJADOR DE LA INTERRUPCIÓN DEL TMR0 -- CONTADOR PARA 10us
void Timer0IntHandler() {
  TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
  TimerDisable(TIMER0_BASE, TIMER_A);
  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);

}

// MANEJADOR DE LA INTERRUPCIÓN DEL PUERTO B -- SEÑAL ECHO
void GPIOIntHandler(void) {

  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);
  ACTIVAR_TRIG = false;
  GPIOIntClear(GPIO_PORTB_BASE, GPIO_INT_PIN_1);

  if (GPIOPinRead(GPIO_PORTB_BASE, GPIO_PIN_1) == GPIO_PIN_1) {
    HWREG(TIMER2_BASE + TIMER_O_TAV) = 0;
    TimerEnable(TIMER2_BASE, TIMER_A);
  } else {
    TimerDisable(TIMER2_BASE, TIMER_A);
    DURACION_ECHO_CICLOS = TimerValueGet(TIMER2_BASE, TIMER_A);
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
            GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2 | GPIO_PIN_3, 2);
        }
        else if ((DISTANCIA_PROMEDIO >= 26) && (DISTANCIA_PROMEDIO <= 50)){
            GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2 | GPIO_PIN_3, 4);
        }
        else if ((DISTANCIA_PROMEDIO >= 51) && (DISTANCIA_PROMEDIO <= 400)){
            GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2 | GPIO_PIN_3, 8);
        }

// PARTE ENTERA Y PARTE DECIMAL
        PENT_DISTANCIA = (int) DISTANCIA_PROMEDIO;
        PDEC_DISTANCIA = (DISTANCIA_PROMEDIO - PENT_DISTANCIA) * 10;

// CONVERSIÓN A CADENA DE CARACTERES
        ltoa(PENT_DISTANCIA, ARRAY_PENT_DISTANCIA, 10);
        ltoa(PDEC_DISTANCIA, ARRAY_PDEC_DISTANCIA, 10);

// PSEUDO CONCATENACIÓN PARA MOSTRAR EN LA TERMINAL
        UARTCharPut(UART3_BASE, ARRAY_PENT_DISTANCIA[0]);
        UARTCharPut(UART3_BASE, ARRAY_PENT_DISTANCIA[1]);
        UARTCharPut(UART3_BASE, ARRAY_PENT_DISTANCIA[2]);
        UARTCharPut(UART3_BASE, '.');
        UARTCharPut(UART3_BASE, ARRAY_PDEC_DISTANCIA[0]);
        TerminalText(" cm");
        DISTANCIA_PROMEDIO = 0;
      }
    }
    else{
        GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1|GPIO_PIN_2 | GPIO_PIN_3, 2);
    }
    ACTIVAR_TRIG = true;
  }
}

// MANEJADOR TX DEL MODULO UART0 -- ACTIVA O DESACTIVA LAS MEDICIONES
void UARTIntHandler(void) {
  ui32Status = UARTIntStatus(UART3_BASE, true);
  UARTIntClear(UART3_BASE, ui32Status);
  while (UARTCharsAvail(UART3_BASE)) {
    RX[0] = UARTCharGet(UART3_BASE);
    RX[1] = UARTCharGetNonBlocking(UART3_BASE);
    UARTCharPut(UART3_BASE, RX[0]);
    if (RX[0]== '1'){
        ACTIVAR_TRIG = true;
    }
    if (RX[0]== '2'){
        ACTIVAR_TRIG = false;
    }
    UARTCharPut(UART3_BASE, '\r');
    UARTCharPut(UART3_BASE, '\n');

  }
}
int main(void) {
// OSCILADOR 80Mhz
  SysCtlClockSet(SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_SYSDIV_2_5);
// CONFIGURACIÓN DEL LA INTERRUPCION PARA DETECTAR EL ECHO
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
  GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_1);
  GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
  GPIOIntEnable(GPIO_PORTB_BASE, GPIO_INT_PIN_1);
  GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_1, GPIO_BOTH_EDGES);
  IntPrioritySet(INT_GPIOB, 0);
  IntRegister(INT_GPIOB, GPIOIntHandler);
// CONFIGURACIÓ DEL PIN PARA EL PULSO TRIGER
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_0);
  // CONFIGURACIÓ DEL PUERTO F PARA LOS INDICADORES
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 );
// CONFIGURACIÓN DEL UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
        SysCtlPeripheralEnable(SYSCTL_PERIPH_UART3);
        GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_6 | GPIO_PIN_7);
        GPIOPinConfigure(GPIO_PC6_U3RX);
        GPIOPinConfigure(GPIO_PC7_U3TX);
        UARTConfigSetExpClk(UART3_BASE,SysCtlClockGet(),9600,(UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE));
        IntMasterEnable();
        IntEnable(INT_UART3);
        UARTIntEnable(UART3_BASE, UART_INT_RX | UART_INT_RT);
// CONFIGURACIÓN DEL TMR0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
  TimerConfigure(TIMER0_BASE, TIMER_CFG_ONE_SHOT_UP);
  TimerLoadSet(TIMER0_BASE, TIMER_A, 800);
  TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
  TimerIntRegister(TIMER0_BASE, TIMER_A, Timer0IntHandler);
  IntEnable(INT_TIMER0A);
  //TimerEnable(TIMER0_BASE, TIMER_A);
// CONFIGURACIÓN DEL TMR2
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
  TimerConfigure(TIMER2_BASE, TIMER_CFG_ONE_SHOT_UP);
  //HWREG(TIMER2_BASE + TIMER_O_TAV) = 0;
  //TimerEnable(TIMER2_BASE, TIMER_A);
  //TimerValueGet(TIMER2_BASE, TIMER_A);
  IntMasterEnable();
  GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);
  GPIOPinWrite(GPIO_PORTF_BASE,GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 , 0);
  while (1) {

    if (ACTIVAR_TRIG) {
      GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_PIN_0);
      TimerEnable(TIMER0_BASE, TIMER_A);
      IntEnable(INT_TIMER0A);
      IntEnable(INT_GPIOB);
    }

  }
}
