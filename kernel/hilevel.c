/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t pcb[ 3 ];
heap* queue;
int n = sizeof(pcb)/sizeof(pcb[0]);
int executing;
int toggle = 0;

extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;
extern void     main_P4b();
extern uint32_t tos_P4b;


// void calcWaitingTime(heap* h) {
//   h->heapArray[0].wt = 0;
//   for (int i = 1; i < h->heapSize - 1; i++) {
//     h->heapArray[i].wt = h->heapArray[i-1].bt + h->heapArray[i-1].wt;
//   }
// }

void hilevel_handler_rst(ctx_t* ctx            ) {
  /* Configure the mechanism for interrupt handling by
   *
   * - configuring timer st. it raises a (periodic) interrupt for each
   *   timer tick,
   * - configuring GIC st. the selected interrupts are forwarded to the
   *   processor via the IRQ interrupt signal, then
   * - enabling IRQ interrupts.
   */

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  /* Initialise PCBs representing processes stemming from execution of
   * the two user programs.  Note in each case that
   *
   * - the CPSR value of 0x50 means the processor is switched into USR
   *   mode, with IRQ interrupts enabled, and
   * - the PC and SP values matche the entry point and top of stack.
   */

   // P3 PCB
   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
   pcb[ 0 ].pid      = 0;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );
   pcb[ 0 ].priority = 3;
   pcb[ 0 ].bt = 5;
   pcb[ 0 ].wt = 0;

   // P4 PCB
   memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
   pcb[ 1 ].pid      = 1;
   pcb[ 1 ].status   = STATUS_READY;
   pcb[ 1 ].ctx.cpsr = 0x50;
   pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
   pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P4  );
   pcb[ 1 ].priority = 6;
   pcb[ 1 ].bt = 4;
   pcb[ 1 ].wt = 0;

   // P4b PCB
   memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
   pcb[ 2 ].pid      = 2;
   pcb[ 2 ].status   = STATUS_READY;
   pcb[ 2 ].ctx.cpsr = 0x50;
   pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P4b );
   pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_P4b  );
   pcb[ 2 ].priority = 8;
   pcb[ 2 ].bt = 2;
   pcb[ 2 ].wt = 0;

  uint8_t* x = ( uint8_t* )( malloc( 10 ) );

  queue = newHeap();
  queue->heapArray = pcb;
  queue->heapSize += sizeof(pcb)/sizeof(pcb[0]);
  queue->arraySize += sizeof(pcb)/sizeof(pcb[0]);
  buildMaxHeap(queue);

  memcpy( ctx, &queue->heapArray[0].ctx, sizeof( ctx_t ) );
  queue->heapArray[0].status = STATUS_EXECUTING;

  int_enable_irq();

  return;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void scheduler( ctx_t* ctx, heap* h) {

  for (int i = 1; i <= h->heapSize - 1; i++) {
    h->heapArray[i].wt++;
  }

   // Every 10 clock cycles
   if (toggle % h->heapArray[0].bt == 0) {

     for (int i = 1; i <= h->heapSize - 1; i++) {
       h->heapArray[i].priority += h->heapArray[i].bt * h->heapArray[i].wt;
     }
   // Reset active process
     h->heapArray[0].priority = 0;
     h->heapArray[0].wt = 0;
   // Preserve
     memcpy( &pcb[ executing ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_3
     pcb[ executing ].status = STATUS_READY;                // update   P_3 status
  // Rebuild heap
     buildMaxHeap(h);
  // Change process
     memcpy( ctx, &h->heapArray[0].ctx, sizeof( ctx_t ) ); // restore  P_4
     h->heapArray[0].status = STATUS_EXECUTING;            // update   P_4 status
  }

  return;
}

// void cycleScheduler( ctx_t* ctx) {
//     memcpy( &pcb[ executing ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_3
//     pcb[ executing ].status = STATUS_READY;                // update   P_3 status
//     memcpy( ctx, &pcb[ (executing+1)%n ].ctx, sizeof( ctx_t ) ); // restore  P_4
//     pcb[ (executing+1)%n ].status = STATUS_EXECUTING;            // update   P_4 status
//     executing = (executing+1)%n;                                 // update   index => P_4
//   return;
// }

// void originalScheduler( ctx_t* ctx ) {
//   if     ( 0 == executing ) {
//     memcpy( &pcb[ 0 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_1
//     pcb[ 0 ].status = STATUS_READY;                // update   P_1 status
//     memcpy( ctx, &pcb[ 1 ].ctx, sizeof( ctx_t ) ); // restore  P_2
//     pcb[ 1 ].status = STATUS_EXECUTING;            // update   P_2 status
//     executing = 1;                                 // update   index => P_2
//   }
//   else if( 1 == executing ) {
//     memcpy( &pcb[ 1 ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_2
//     pcb[ 1 ].status = STATUS_READY;                // update   P_2 status
//     memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) ); // restore  P_3
//     pcb[ 0 ].status = STATUS_EXECUTING;            // update   P_3 status
//     executing = 0;                                 // update   index => P_3
//   }
//   return;
// }

// void randomScheduler( ctx_t* ctx ) {
//     int nextProcess = rand() % n;
//     if (nextProcess != executing) {
//     memcpy( &pcb[ executing ].ctx, ctx, sizeof( ctx_t ) ); // preserve P_3
//     pcb[ executing ].status = STATUS_READY;                // update   P_3 status
//     memcpy( ctx, &pcb[ nextProcess ].ctx, sizeof( ctx_t ) ); // restore  P_4
//     pcb[ nextProcess ].status = STATUS_EXECUTING;            // update   P_4 status
//     executing = nextProcess;
//     }                            // update   index => P_4
//   return;
// }

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void hilevel_handler_irq(ctx_t* ctx ) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;
  toggle++;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    //PL011_putc( UART0, 'T', true );
    TIMER0->Timer1IntClr = 0x01;
    scheduler(ctx, queue);
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identified encoded as an immediate operand in the
   * instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call,
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    // case 0x00 : { // 0x00 => yield()
    //   scheduler( ctx );
    //   break;
    // }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }

   case 0x02 : { // 0x02 => read( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        *x++ = PL011_getc( UART0, true );
      }

/*      x -= 5;

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }
*/

      ctx->gpr[ 0 ] = n;
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
