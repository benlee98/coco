/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "P4b.h"

uint32_t gcdb( uint32_t x, uint32_t y ) {
  if     ( x == y ) {
    return x;
  }
  else if( x >  y ) {
    return gcdb( x - y, y );
  }
  else if( x <  y ) {
    return gcdb( x, y - x );
  }
}

void main_P4b() {
  char * x;
  int id = fork();
  itoa(x, id);
  while( 1 ) {
    write( STDOUT_FILENO, x, 3 );

    uint32_t lo = 1 <<  4;
    uint32_t hi = 1 <<  8;

    for( uint32_t x = lo; x < hi; x++ ) {
      for( uint32_t y = lo; y < hi; y++ ) {
        uint32_t r = gcdb( x, y );
      }
    }
  }

  exit( EXIT_SUCCESS );
}
