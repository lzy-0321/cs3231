#include "opt-synchprobs.h"
#include "counter.h"
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>


/*
 * Declare the counter variable that all threads increment or decrement
 * via the interface provided here.
 *
 * Declaring it "volatile" instructs the compiler to always (re)read the
 * variable from memory and not to optimise by keeping the value in a process 
 * register and avoid memory references.
 *
 * NOTE: The volatile declaration is actually not needed for the provided code 
 * as the variable is only loaded once in each function.
 */

static volatile int the_counter;
static struct lock *lock_counter;
/*
 * ********************************************************************
 * INSERT ANY GLOBAL VARIABLES YOU REQUIRE HERE
 * ********************************************************************
 */


void counter_increment(void)
{
        lock_acquire(lock_counter);
        the_counter++;
        lock_release(lock_counter);
}

void counter_decrement(void)
{
        lock_acquire(lock_counter);
        the_counter--;
        lock_release(lock_counter);
}

int counter_initialise(int val)
{
        the_counter = val;

        /*
         * ********************************************************************
         * INSERT ANY INITIALISATION CODE YOU REQUIRE HERE
         * ********************************************************************
         */
        lock_counter = lock_create("lock_counter");
        if (lock_counter == NULL) {
                return ENOMEM;
        }
        /*
         * Return 0 to indicate success
         * Return non-zero to indicate error.
         * e.g. 
         * return ENOMEM
         * indicates an allocation failure to the caller 
         */
        
        return 0;
}

int counter_read_and_destroy(void)
{
        /*
         * **********************************************************************
         * INSERT ANY CLEANUP CODE YOU REQUIRE HERE
         * **********************************************************************
         */
        int current_value;
        lock_acquire(lock_counter);
        current_value = the_counter;
        the_counter = 0;
        lock_release(lock_counter);
        lock_destroy(lock_counter);
        return current_value;
}
