#include "opt-synchprobs.h"
#include "kitchen.h"
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>




/*
 * ********************************************************************
 * INSERT ANY GLOBAL VARIABLES YOU REQUIRE HERE
 * ********************************************************************
 */
static struct cv *cv_full;
static struct cv *cv_empty;
static struct lock *lock_cook;
static int count;

/*
 * initialise_kitchen: 
 *
 * This function is called during the initialisation phase of the
 * kitchen, i.e.before any threads are created.
 *
 * Initialise any global variables or create any synchronisation
 * primitives here.
 * 
 * The function returns 0 on success or non-zero on failure.
 */

int initialise_kitchen()
{
        cv_full = cv_create("cv_full");
        if (cv_full == NULL) {
                return ENOMEM;
        }
        cv_empty = cv_create("cv_empty");
        if (cv_empty == NULL) {
                return ENOMEM;
        }
        lock_cook = lock_create("lock_cook");
        if (lock_cook == NULL) {
                return ENOMEM;
        }
        count = 0;
        return 0;
}

/*
 * cleanup_kitchen:
 *
 * This function is called after the dining threads and cook thread
 * have exited the system. You should deallocated any memory allocated
 * by the initialisation phase (e.g. destroy any synchronisation
 * primitives).
 */

void cleanup_kitchen()
{
        cv_destroy(cv_full);
        cv_destroy(cv_empty);
        lock_destroy(lock_cook);
}

/*
 * do_cooking:
 *
 * This function is called repeatedly by the cook thread to provide
 * enough soup to dining customers. It creates soup by calling
 * cook_soup_in_pot().
 *
 * It should wait until the pot is empty before calling
 * cook_soup_in_pot().
 *
 * It should wake any dining threads waiting for more soup.
 */

void do_cooking()
{       
        lock_acquire(lock_cook);
        while (count > 0) {
                cv_wait(cv_empty, lock_cook);
        }
        cook_soup_in_pot();
        count = POTSIZE_IN_SERVES;
        cv_broadcast(cv_full, lock_cook);
        lock_release(lock_cook);
}

/*
 * fill_bowl:
 *
 * This function is called repeatedly by dining threads to obtain soup
 * to satify their hunger. Dining threads fill their bowl by calling
 * get_serving_from_pot().
 *
 * It should wait until there is soup in the pot before calling
 * get_serving_from_pot().
 *
 * get_serving_from_pot() should be called mutually exclusively as
 * only one thread can fill their bowl at a time.
 *
 * fill_bowl should wake the cooking thread if there is no soup left
 * in the pot.
 */

void fill_bowl()
{
        lock_acquire(lock_cook);
        while (count == 0) {
                cv_signal(cv_empty, lock_cook);
                cv_wait(cv_full, lock_cook);
        }
        get_serving_from_pot();
        count--;
        lock_release(lock_cook);
}