/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

#define SYS_YIELD     ( 0x00 )
#define SYS_WRITE     ( 0x01 )
#define SYS_READ      ( 0x02 )
#define SYS_FORK      ( 0x03 )
#define SYS_EXIT      ( 0x04 )
#define SYS_EXEC      ( 0x05 )
#define SYS_KILL      ( 0x06 )
#define SYS_NICE      ( 0x07 )

pcb_t pcb[ 30 ];
heap* queue;
// int n = sizeof(pcb)/sizeof(pcb[0]);
int size = 0;
int executing;
int toggle = 0;
int programs = 0;

extern void     main_P3();
extern void     main_P4();
extern void     main_P4b();
extern void main_console();

extern uint32_t to_user_p;

uint32_t* to_console = &to_user_p + (0*0x00001000);
uint32_t* tos_P3 = &to_user_p + (0*0x00001000);
uint32_t* tos_P4 = &to_user_p + (1*0x00001000);
uint32_t* tos_P4b = &to_user_p + (2*0x00001000);

// void calcWaitingTime(heap* h) {
//   h->heapArray[0].wt = 0;
//   for (int i = 1; i < h->heapSize - 1; i++) {
//     h->heapArray[i].wt = h->heapArray[i-1].bt + h->heapArray[i-1].wt;
//   }
// }

void hilevel_handler_rst(ctx_t* ctx            ) {

 uint8_t* x = ( uint8_t* )( malloc( 10 ) );
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
   pcb[ 0 ].ctx.sp   = ( uint32_t )( to_console  );
   pcb[ 0 ].priority = 3;
   pcb[ 0 ].bt = 1;
   pcb[ 0 ].wt = 0;
   size++;

   // // P3 PCB
   // memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
   // pcb[ 0 ].pid      = 0;
   // pcb[ 0 ].status   = STATUS_READY;
   // pcb[ 0 ].ctx.cpsr = 0x50;
   // pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
   // pcb[ 0 ].ctx.sp   = ( uint32_t )( tos_P3  );
   // pcb[ 0 ].priority = 3;
   // pcb[ 0 ].bt = 5;
   // pcb[ 0 ].wt = 0;
   // size++;
   //
   // // P4 PCB
   // memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
   // pcb[ 1 ].pid      = 1;
   // pcb[ 1 ].status   = STATUS_READY;
   // pcb[ 1 ].ctx.cpsr = 0x50;
   // pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
   // pcb[ 1 ].ctx.sp   = ( uint32_t )( tos_P4  );
   // pcb[ 1 ].priority = 6;
   // pcb[ 1 ].bt = 4;
   // pcb[ 1 ].wt = 0;
   // size++;
   //
   // // P4b PCB
   // memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
   // pcb[ 2 ].pid      = 2;
   // pcb[ 2 ].status   = STATUS_READY;
   // pcb[ 2 ].ctx.cpsr = 0x50;
   // pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P4b );
   // pcb[ 2 ].ctx.sp   = ( uint32_t )( tos_P4b  );
   // pcb[ 2 ].priority = 8;
   // pcb[ 2 ].bt = 2;
   // pcb[ 2 ].wt = 0;
   // size++;

   queue = newHeap();
   queue->heapArray = pcb;
   queue->heapSize+=size;
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
    case SYS_WRITE : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }

   case SYS_READ : { // 0x02 => read( fd, x, n )
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

    case SYS_FORK : { // 0x03 => fork()
      int q = queue->heapSize;
      memset( &pcb[ q ], 0, sizeof( pcb_t ) );
      ctx_t* new_ctx = ctx;
      void* old_address = (uint32_t*) &to_user_p - ((q-1) * 0x00001000);
      void* new_address = (uint32_t*) &to_user_p - (q * 0x00001000);
      pcb[ q ].pid      = q;
      pcb[ q ].status   = pcb[ 0 ].status;
      pcb[ q ].ctx.cpsr = new_ctx->cpsr;
      pcb[ q ].ctx.pc   = new_ctx->pc;

      pcb[ q ].ctx.sp   = (uint32_t)&new_address;
      memcpy(new_address, old_address, (uint32_t) old_address - ctx->sp);

      pcb[ q ].ctx.lr   = new_ctx->lr;
      pcb[ q ].priority = pcb[ 0 ].priority;
      pcb[ q ].bt       = pcb[ 0 ].bt;
      pcb[ q ].wt       = pcb[ 0 ].wt;
      // increment size of queue variable

      // Copying gpr across
      int i = sizeof(pcb[ 0 ].ctx.gpr)/sizeof(pcb[ 0 ].ctx.gpr[0]);
      for (int j = 0; j < i; j++) {
        pcb[ q ].ctx.gpr[ j ] = new_ctx->gpr[ j ];
      }
      pcb[ q ].ctx.gpr[ 0 ]  = 0;
      ctx->gpr[ 0 ] = pcb[ q ].pid;
      size++;
      queue->heapSize++;
      break;
    }

    // case SYS_EXIT : { // 0x04 => exit()
    //
    // }

    case SYS_EXEC : { // 0x05 => exec()
      int q = queue->heapSize;
      ctx->lr = ( uint32_t )(ctx->gpr[0]);
      break;
    }

    case SYS_KILL : { // 0x06 => kill()
      int q = queue->heapSize;
      int pid = ctx->gpr[0];
      int sig = ctx->gpr[1];
        for (int i = 0; i < q; i++) {
          if (pcb[i].pid == pid) {
            pcb[i].priority = INT_MIN;
            pcb[i].status = STATUS_TERMINATED;
            buildMaxHeap(queue);
            queue->heapSize--;
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
