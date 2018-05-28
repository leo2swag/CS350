#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

static int volatile ways[4][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};

bool hit_happen(int volatile check[]);
bool intersection_no_hit(Direction origin, Direction destination);
void one_intersection_end(Direction origin, Direction destination);
/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersectionLock;
static struct cv *intersectionCv;



/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
	intersectionLock = lock_create("intersectionLock");
	if (intersectionLock == NULL) {
		panic("could not create intersection lock");
	}
	intersectionCv = cv_create("intersectionCv");
	if (intersectionCv == NULL) {
		panic("could not create intersection cv");
	}
	return;
}


/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCv != NULL);
	lock_destroy(intersectionLock);
	cv_destroy(intersectionCv);
}

bool
intersection_no_hit(Direction origin, Direction destination) {
	if (ways[1][2] ||
		ways[1][3] ||
		ways[2][0] ||
		ways[2][1] ||
		ways[2][3] ||
		ways[3][0] ||
		ways[3][1]
		) {
			return false;
		} else {
			ways[0][1]++;
		}
		return true;
		(void)origin;
		(void)destination;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCv != NULL);
	lock_acquire(intersectionLock);
	while (!intersection_no_hit(origin, destination)) {
		cv_wait(intersectionCv, intersectionLock);
		
	}
	cv_signal(intersectionCv, intersectionLock);
	lock_release(intersectionLock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
	KASSERT(intersectionLock != NULL);
	KASSERT(intersectionCv != NULL);

	lock_acquire(intersectionLock);
	//one_intersection_end(origin, destination);
	ways[origin][destination]--;
	cv_signal(intersectionCv, intersectionLock);
	lock_release(intersectionLock);
}
