// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Seminario 2. Introducción a los monitores en C++11.
//
// archivo: m_prodcons1_sc.cpp
//
// Ejemplo de un monitor en C++11 con semántica SC, para el problema
// del productor/consumidor, con múltiples productores y consumidores.
// Opcion LIFO (stack)
//
// -----------------------------------------------------------------------------


#include <iostream>
#include <iomanip>
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>

using namespace std ;

const int
   num_items  = 40 ;     // número de items a producir/consumir
const int num_prod = 4; // número de productores   
const int num_cons = 5; // número de consumidores
mutex
   mtx ;                 // mutex de escritura en pantalla
unsigned
    contador_p[num_prod], // contador de items producidos por cada productor
   cont_prod[num_items], // contadores de verificación: producidos
   cont_cons[num_items]; // contadores de verificación: consumidos

//**********************************************************************
// plantilla de función para generar un entero aleatorio uniformemente
// distribuido entre dos valores enteros, ambos incluidos
// (ambos tienen que ser dos constantes, conocidas en tiempo de compilación)
//----------------------------------------------------------------------

template< int min, int max > int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max ) ;
  return distribucion_uniforme( generador );
}

//**********************************************************************
// funciones comunes a las dos soluciones (fifo y lifo)
//----------------------------------------------------------------------

int producir_dato(unsigned num_hebra)
{
   static int contador = 0 ;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));
   mtx.lock();
   cout << "producido: " << contador << " por hebra " << num_hebra << endl << flush ;
   contador_p[num_hebra]++;
   cout << "hebra " << num_hebra << " ha producido " << contador_p[num_hebra] << " items " << endl;
   mtx.unlock();
   cont_prod[contador] ++ ;
   return contador++ ;
}
//----------------------------------------------------------------------

void consumir_dato( unsigned dato, unsigned num_hebra )
{
   if ( num_items <= dato )
   {
      cout << " dato === " << dato << ", num_items == " << num_items << endl ;
      assert( dato < num_items );
   }
   cont_cons[dato] ++ ;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));
   mtx.lock();
   cout << "                  consumido: " << dato << " por hebra " << num_hebra << endl ;
   mtx.unlock();
}
//----------------------------------------------------------------------

void ini_contadores()
{
   for( unsigned i = 0 ; i < num_items ; i++ )
   {  cont_prod[i] = 0 ;
      cont_cons[i] = 0 ;
   }
   for(unsigned i = 0; i < num_prod; i++)
    contador_p[i] = 0;
}

//----------------------------------------------------------------------

void test_contadores()
{
   bool ok = true ;
   cout << "comprobando contadores ...." << flush ;

   for( unsigned i = 0 ; i < num_items ; i++ )
   {
      if ( cont_prod[i] != 1 )
      {
         cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
         ok = false ;
      }
      if ( cont_cons[i] != 1 )
      {
         cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
         ok = false ;
      }
   }
   if (ok)
      cout << endl << flush << "solución (aparentemente) correcta." << endl << flush ;
}

// *****************************************************************************
// clase para monitor buffer, version LIFO, semántica SC, un prod. y un cons.

class ProdCons1SC
{
 private:
 static const int           // constantes:
   num_celdas_total = 10;   //  núm. de entradas del buffer
 int                        // variables permanentes
   buffer[num_celdas_total],//  buffer de tamaño fijo, con los datos
   primera_libre ;          //  indice de celda de la próxima inserción
 mutex
   cerrojo_monitor ;        // cerrojo del monitor
 condition_variable         // colas condicion:
   ocupadas,                //  cola donde espera el consumidor (n>0)
   libres ;                 //  cola donde espera el productor  (n<num_celdas_total)

 public:                    // constructor y métodos públicos
   ProdCons1SC(  ) ;           // constructor
   int  leer();                // extraer un valor (sentencia L) (consumidor)
   void escribir( int valor ); // insertar un valor (sentencia E) (productor)
} ;
// -----------------------------------------------------------------------------

ProdCons1SC::ProdCons1SC(  )
{
   primera_libre = 0 ;
}
// -----------------------------------------------------------------------------
// función llamada por el consumidor para extraer un dato

int ProdCons1SC::leer(  )
{
   // ganar la exclusión mutua del monitor con una guarda
   unique_lock<mutex> guarda( cerrojo_monitor );

   // esperar bloqueado hasta que 0 < num_celdas_ocupadas
   while ( primera_libre == 0 )
      ocupadas.wait( guarda );

   // hacer la operación de lectura, actualizando estado del monitor
   assert( 0 < primera_libre  );
   primera_libre-- ;
   const int valor = buffer[primera_libre] ;


   // señalar al productor que hay un hueco libre, por si está esperando
   libres.notify_one();

   // devolver valor
   return valor ;
}
// -----------------------------------------------------------------------------

void ProdCons1SC::escribir( int valor )
{
   // ganar la exclusión mutua del monitor con una guarda
   unique_lock<mutex> guarda( cerrojo_monitor );

   // esperar bloqueado hasta que num_celdas_ocupadas < num_celdas_total
   while ( primera_libre == num_celdas_total )
      libres.wait( guarda );

   //cout << "escribir: ocup == " << num_celdas_ocupadas << ", total == " << num_celdas_total << endl ;
   assert( primera_libre < num_celdas_total );

   // hacer la operación de inserción, actualizando estado del monitor
   buffer[primera_libre] = valor ;
   primera_libre++ ;

   // señalar al consumidor que ya hay una celda ocupada (por si esta esperando)
   ocupadas.notify_one();
}
// *****************************************************************************
// funciones de hebras

void funcion_hebra_productora( ProdCons1SC * monitor, unsigned num_hebra )
{
   for( unsigned i = 0 ; i < num_items/num_prod ; i++ )
   {
      int valor = producir_dato(num_hebra) ;
      monitor->escribir( valor );
   }
}
// -----------------------------------------------------------------------------

void funcion_hebra_consumidora( ProdCons1SC * monitor, unsigned num_hebra )
{
   for( unsigned i = 0 ; i < num_items/num_cons ; i++ )
   {
      int valor = monitor->leer();
      consumir_dato( valor, num_hebra ) ;
   }
}
// -----------------------------------------------------------------------------

int main()
{
   
   cout << "-------------------------------------------------------------------------------" << endl
        << "Problema de los productores-consumidores (múltiples prod/cons, Monitor SC, buffer LIFO). " << endl
        << "-------------------------------------------------------------------------------" << endl
        << flush ;

   ProdCons1SC monitor ;

   thread hebra_productora [num_prod], hebra_consumidora [num_cons];

    for (unsigned i = 0; i < num_prod; i++)
       hebra_productora[i] = thread ( funcion_hebra_productora, &monitor, i);

    for (unsigned i = 0; i < num_cons; i++)
       hebra_consumidora[i] = thread ( funcion_hebra_consumidora, &monitor, i);


   for (unsigned i = 0; i < num_prod; i++)
       hebra_productora[i].join();

    for (unsigned i = 0; i < num_cons; i++)
       hebra_consumidora[i].join();


   // comprobar que cada item se ha producido y consumido exactamente una vez
   test_contadores() ;
}