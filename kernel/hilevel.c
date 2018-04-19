/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

pcb_t pcb[ 30 ];
int n = sizeof(pcb)/sizeof(pcb[0]);
int processes = 0;
int executing = 0;
int toggle = 0;

// Shared memory addresses
int shm_address[30];

// Initialise number of open shared memories
int shm_num = 0;


extern void     main_P3();
extern void     main_P4();
extern void     main_P4b();
extern void main_console();

extern uint32_t tos_user_p;
extern uint32_t tos_shm;

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

   // console PCB initialisation
   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
   pcb[ 0 ].pid      = 0;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_user_p  );
   pcb[ 0 ].priority = 3;
   pcb[ 0 ].bt = 3;
   pcb[ 0 ].wt = 0;
   processes++;

   memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );

   int_enable_irq();

   return;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

 void scheduler( ctx_t* ctx, bool force ) {
// Increment waiting times of processes that are ready but not  currently
// executing
   for (int i = 0; i < processes; i++) {
     if (pcb[i].status == STATUS_READY) {
       pcb[i].wt++;
     }
   }

// If the burst time of executing process has elapsed, update other processes'
// priorities
   if (toggle % pcb[executing].bt == 0 || force) {
     for (int i = 0; i < processes; i++) {
       if (pcb[i].status == STATUS_READY) {
         // Increase priority of other processes
         pcb[i].priority += 1 + (pcb[i].bt * pcb[i].wt);
       }
     }

     // Reset active process
     pcb[executing].priority = 0;
     pcb[executing].wt = 0;

     // Preserve executing process, make ready if executing
     memcpy( &pcb[ executing ].ctx, ctx, sizeof( ctx_t ) );
     if(pcb[executing].status == STATUS_EXECUTING) {
        pcb[ executing ].status = STATUS_READY;
     }
     int max = executing;

     // Find next process using max priority
     for (int i = 0; i < processes; i++) {
       if (pcb[i].priority >= pcb[max].priority && pcb[i].status == STATUS_READY) {
         max = i;
       }
     }

     // Restore next process to execute
     if (max != executing && pcb[max].status == STATUS_READY) {
       memcpy( ctx, &pcb[max].ctx, sizeof( ctx_t ) );
     }
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
    //PL011_putc( UART0, 'T', true );
    TIMER0->Timer1IntClr = 0x01;
    scheduler(ctx, false);
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identified encoded as an immediate operand in the
   * instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call,
   * - write any return value back to preserved usr mode registers.
   */
  //PL011_putc( UART0, 'S', true );

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

      ctx->gpr[ 0 ] = n;
      break;
    }

    case 0x03 : { // 0x03 => fork()
      int newIndex = processes;

      memcpy(&pcb[processes].ctx, ctx, sizeof(ctx_t));

      pcb[ newIndex ].pid      = newIndex;
      pcb[ newIndex ].status   = STATUS_READY;
      pcb[ newIndex ].ctx.gpr[ 0 ] = 0;
      pcb[ newIndex ].priority = pcb[ executing ].priority;
      pcb[ newIndex ].bt       = pcb[ executing ].bt;
      pcb[ newIndex ].wt       = pcb[ executing ].wt;

      uint32_t old_tos = (uint32_t)(&tos_user_p) - (executing * 0x00001000);
      uint32_t new_tos = (uint32_t)(&tos_user_p) - (processes * 0x00001000);

      uint32_t correction = old_tos - ctx->sp;
      uint32_t new_sp = new_tos - correction;

      pcb[newIndex].ctx.sp = new_sp;

      memcpy((uint32_t*) (new_tos - correction), (uint32_t*) (old_tos - correction), correction);

      processes++;
      ctx->gpr[ 0 ] = pcb[ newIndex ].pid;
      break;
    }

    case 0x04 : { // 0x04 => exit()
      memset(&pcb[executing], 0, sizeof(pcb_t));
      pcb[executing].priority = INT_MIN;
      pcb[executing].status = STATUS_TERMINATED;
      scheduler(ctx, true);
      break;
    }

    case 0x05 : { // 0x05 => exec()
      if (ctx->gpr[0]) {
        PL011_putc( UART0, 'X', true );
        ctx->pc = ( uint32_t )(ctx->gpr[0]);
      } else {
        PL011_putc( UART0, 'N', true );
        PL011_putc( UART0, '/', true );
        PL011_putc( UART0, 'A', true );
        memset(&pcb[executing], 0, sizeof(pcb_t));
        pcb[executing].priority = INT_MIN;
        pcb[executing].status = STATUS_TERMINATED;
        scheduler(ctx, true);
      }
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

    case 0x07: { // SYS_NICE
      PL011_putc( UART0, '+', true );
      PL011_putc( UART0, '+', true );
      int pid = ctx->gpr[0];
      int inc = ctx->gpr[1];
      pcb[pid].priority += inc;
      break;
    }

    case 0x08: { // SHM_MKE
      int size = ctx->gpr[0];
      shm_address[shm_num] = (uint32_t) &tos_shm - (size);
      ctx->gpr[0] = shm_num++;
      break;
    }

    case 0x09: { // SHM_GET
      int id = ctx->gpr[0];
      ctx->gpr[0] = shm_address[id];
      break;
    }

    case 0x0B: { // SYS_WAIT
      pcb[executing].status = STATUS_WAITING;
      scheduler(ctx, true);
      break;
    }

    case 0x0C: { // SYS_UNWAIT
      int id = ctx->gpr[0];
      pcb[id].status = STATUS_READY;
      break;
    }

    case 0x0D: { // SYS_PID
      ctx->gpr[0] = pcb[executing].pid;
      break;
    }

    case 0x0E: { // SYS_BURST
      PL011_putc( UART0, 'B', true );
      int pid = ctx->gpr[0];
      int inc = ctx->gpr[1];
      pcb[pid].bt += inc;
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
