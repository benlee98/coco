/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t pcb[ 30 ];
// heap* queue;
int n = sizeof(pcb)/sizeof(pcb[0]);
int processes = 0;
int executing = 0;
int toggle = 0;

extern void     main_P3();
extern void     main_P4();
extern void     main_P4b();
extern void main_console();

extern uint32_t tos_user_p;

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

   // console PCB
   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
   pcb[ 0 ].pid      = 0;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_user_p  );
   pcb[ 0 ].priority = 3;
   pcb[ 0 ].bt = 1;
   pcb[ 0 ].wt = 0;
   processes++;

   // // P4 PCB
   // memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
   // pcb[ 1 ].pid      = 1;
   // pcb[ 1 ].status   = STATUS_READY;
   // pcb[ 1 ].ctx.cpsr = 0x50;
   // pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
   // pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_user_p + processes*0x1000 );
   // pcb[ 1 ].priority = 6;
   // pcb[ 1 ].bt = 4;
   // pcb[ 1 ].wt = 0;
   // processes++;
   //
   // // P3 PCB
   // memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
   // pcb[ 2 ].pid      = 0;
   // pcb[ 2 ].status   = STATUS_READY;
   // pcb[ 2 ].ctx.cpsr = 0x50;
   // pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P3 );
   // pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_user_p + processes*0x1000 );
   // pcb[ 2 ].priority = 3;
   // pcb[ 2 ].bt = 5;
   // pcb[ 2 ].wt = 0;
   // processes++;


   memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );

   int_enable_irq();

   return;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void scheduler( ctx_t* ctx ) {

  for (int i = 0; i < processes; i++) {
    if (pcb[i].status == STATUS_READY) {
      pcb[i].wt++;
    }
  }

  if (toggle % pcb[executing].bt == 0) {
   for (int i = 0; i < processes; i++) {
     if (pcb[i].status == STATUS_READY) {
       pcb[i].priority += pcb[i].bt * pcb[i].wt;
    }
   }
 // Reset active process
   pcb[executing].priority = 0;
   pcb[executing].wt = 0;
 // Preserve
   memcpy( &pcb[ executing ].ctx, ctx, sizeof( ctx_t ) );
   pcb[ executing ].status = STATUS_READY;

   int max = executing;
   for (int i = 0; i < processes; i++) {
     if (pcb[i].priority >= pcb[max].priority) {
       max = i;
     }
   }
   if (max != executing) {
     memcpy( ctx, &pcb[max].ctx, sizeof( ctx_t ) );
   }
   // Change process
    pcb[max].status = STATUS_EXECUTING;
    executing = max;
  }
  return;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void hilevel_handler_irq(ctx_t* ctx ) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;
  toggle++;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    PL011_putc( UART0, 'T', true );
    TIMER0->Timer1IntClr = 0x01;
    scheduler(ctx);
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
  PL011_putc( UART0, 'S', true );

  switch( id ) {
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

    case 0x03 : { // 0x03 => fork()
      int newIndex = processes;

      void* old_tos = (uint32_t*) &tos_user_p - (executing * 0x00001000);
      void* new_tos = (uint32_t*) &tos_user_p - (processes * 0x00001000);
      memcpy(new_tos, old_tos, (uint32_t) old_tos - ctx->sp);
      memset(&pcb[processes], 0, sizeof(pcb_t));

      pcb[ newIndex ].pid      = newIndex;
      pcb[ newIndex ].status   = pcb[ executing ].status;
      pcb[ newIndex ].ctx.cpsr = ctx->cpsr;
      pcb[ newIndex ].ctx.pc   = ctx->pc;
      pcb[ newIndex ].ctx.sp   = (uint32_t) &new_tos;
      pcb[ newIndex ].ctx.lr   = ctx->lr;
      // Copying gpr across
      int i = sizeof(pcb[ executing ].ctx.gpr)/sizeof(pcb[ executing ].ctx.gpr[0]);
      for (int j = 0; j < i; j++) {
        pcb[ processes ].ctx.gpr[ j ] = ctx->gpr[ j ];
      }

      pcb[ newIndex ].priority = pcb[ executing ].priority;
      pcb[ newIndex ].bt       = pcb[ executing ].bt;
      pcb[ newIndex ].wt       = pcb[ executing ].wt;
      // increment size of queue variable

      pcb[ newIndex ].ctx.gpr[ 0 ] = 0;
      ctx->gpr[ 0 ] = pcb[ newIndex ].pid;

      processes++;
      break;
    }

    case 0x04 : { // 0x04 => exit()
      pcb[executing].priority = INT_MIN;
      pcb[executing].status = STATUS_TERMINATED;
      break;
    }

    case 0x05 : { // 0x05 => exec()
      ctx->lr = ( uint32_t )(ctx->gpr[0]);
      break;
    }

    case 0x06 : { // 0x06 => kill()
      int pid = ctx->gpr[0];
      int sig = ctx->gpr[1];
      for (int i = 0; i < processes; i++) {
        if (pcb[i].pid  == pid) {
          pcb[i].priority = INT_MIN;
          pcb[i].status = STATUS_TERMINATED;
          break;
        }
      }
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
