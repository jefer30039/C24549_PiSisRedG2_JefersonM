/*
 * Suma uno a un total mil veces por cada proceso generado
 * Recibe como parametro la cantidad de procesos que se quiere arrancar
 * Author: Programacion Concurrente (Francisco Arroyo)
 * Version: 2026/Mar/20
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <thread>
#include <mutex>

// Shared variables
long total;
std::mutex mutex;


/*
 *  Do some work, by now add one to a variable
 */
void * AddOneWithMutex( void * param ) {
   int i;
   long myTotal = 0;

   for ( i = 0; i < 1000; i++ ) {
      myTotal++;
//   usleep( 1 );
   }

   mutex.lock();
   total += myTotal;
   mutex.unlock();

   return param;

}


/*
 *  Do some work, by now add one to a variable
 */
void * AddOne( void * param ) {
   int i;

   for ( i = 0; i < 1000; i++ ) {
      total++;
//      usleep( 1 );
   }

   return param;

}


/**
  * Serial test
 **/
long SerialTest( long hilos ) {
   long i, hilo;

   for ( hilo = 0; hilo < hilos; hilo++ ) {

      for ( i = 0; i < 1000; i++ ) {
         total++;			// Suma uno
//         usleep( 1 );
      }

   }

   return total;

}


/*
  Fork test with NO race condition
*/
long ForkTestNoRaceCondition( long hilos ) {
   long hilo;

   std::thread sumadores[ hilos ];

   for ( hilo = 0; hilo < hilos; hilo++ ) {
      sumadores[ hilo ] = std::thread( AddOne, &hilo );
   }

   for ( hilo = 0; hilo < hilos; hilo++ ) {
      sumadores[ hilo ].join();
   }

   return total;

}

/*
  Fork test with race condition
*/
long ForkTestRaceCondition( long hilos ) {
   long hilo;

   std::thread sumadores[ hilos ];

   for ( hilo = 0; hilo < hilos; hilo++ ) {
      sumadores[ hilo ] = std::thread( AddOneWithMutex, &hilo );
   }

   for ( hilo = 0; hilo < hilos; hilo++ ) {
      sumadores[ hilo ].join();
   }

   return total;

}


int main( int argc, char ** argv ) {
   long hilos;
   clock_t start, finish;
   double used;


   hilos = 100;
   if ( argc > 1 ) {
      hilos = atol( argv[ 1 ] );
   }

   start = clock();
   total = 0;
   SerialTest( hilos );
   finish = clock();
   used = ((double) (finish - start)) / CLOCKS_PER_SEC;
   printf( "Serial version:      Valor acumulado es \033[91m %ld \033[0m con %ld hilos in %f seconds\n", total, hilos, used );

   start = clock();
   total = 0;
   ForkTestRaceCondition( hilos );
   finish = clock();
   used = ((double) (finish - start)) / CLOCKS_PER_SEC;
   printf( "PThr, Race Cond.:    Valor acumulado es \033[91m %ld \033[0m con %ld hilos in %f seconds\n", total, hilos, used );

   start = clock();
   total = 0;
   ForkTestNoRaceCondition( hilos );
   finish = clock();
   used = ((double) (finish - start)) / CLOCKS_PER_SEC;
   printf( "PThr, NO Race Cond.:  Valor acumulado es \033[91m %ld \033[0m con %ld hilos in %f seconds\n", total, hilos, used );
}

