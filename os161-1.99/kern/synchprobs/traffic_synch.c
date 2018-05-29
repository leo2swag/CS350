#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

static int volatile ways[4][3] = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};

bool intersection_no_hit(Direction origin, Direction destination);
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
	if (origin == 0 && destination == 1) { //NE
		if (ways[1][2]||	//ES
			ways[1][3]||	//EW
			ways[2][3]||	//SW
			ways[2][1]||	//SE
			ways[2][0]||	//SN
			ways[3][0]||	//WN
			ways[3][1]		//WE
			) {
				return false;
			}
	} else if (origin == 0 && destination == 2) { //NS
		if (ways[1][2]||	//ES
			ways[1][3]||	//EW
			ways[2][3]||	//SW
			ways[3][0]||	//WN
			ways[3][1]||	//WE
			ways[3][2]		//WS
			) {
				return false;
			}
	} else if (origin == 0 && destination == 3) { //NW
		if (ways[2][3]||	//SW
			ways[1][3]		//EW
			) {
				return false;
			}
	} else if (origin == 1 && destination == 0) { //EN
		if (ways[2][0]||	//SN
			ways[3][0]		//WN
			) {
				return false;
			}
	} else if (origin == 1 && destination == 2) { //ES
		if (ways[0][1]||	//NE
			ways[0][2]||	//NS
			ways[2][3]||	//SW
			ways[2][0]||	//SN
			ways[3][2]||	//WS
			ways[3][0]||	//WN
			ways[3][1]		//WE
			) {
				return false;
			}
	} else if (origin == 1 && destination == 3) { //EW
		if (ways[0][1]||	//NE
			ways[0][2]||	//NS
			ways[0][3]||	//NW
			ways[2][3]||	//SW
			ways[2][0]||	//SN
			ways[3][0]		//WN
			) {
				return false;
			}
	} else if (origin == 2 && destination == 0) { //SN
		if (ways[0][1]||	//NE
			ways[1][2]||	//ES
			ways[1][3]||	//EW
			ways[1][0]||	//EN
			ways[3][0]||	//WN
			ways[3][1]		//WE
			) {
				return false;
			}
	} else if (origin == 2 && destination == 1) { //SE
		if (ways[3][1]||	//WE
			ways[0][1]		//NE
			) {
				return false;
			} 
	} else if (origin == 2 && destination == 3) { //SW
		if (ways[0][1]||	//NE
			ways[0][2]||	//NS
			ways[1][2]||	//ES
			ways[1][3]||	//EW
			ways[0][3]||	//NW
			ways[3][1]||	//WE
			ways[3][0]		//WN
			) {
				return false;
			}
	} else if (origin == 3 && destination == 0) { //WN
		if (ways[0][1]||	//NE
			ways[0][2]||	//NS
			ways[1][2]||	//ES
			ways[1][3]||	//EW
			ways[1][0]||	//EN
			ways[2][3]||	//SW
			ways[2][0]		//SN
			) {
				return false;
			}
	} else if (origin == 3 && destination == 1) { //WE
		if (ways[0][1]||	//NE
			ways[0][2]||	//NS
			ways[1][2]||	//ES
			ways[2][3]||	//SW
			ways[2][0]||	//SN
			ways[2][1]		//SE
			) {
				return false;
			}
	} else if (origin == 3 && destination == 2) { //WS
		if (ways[0][2]||	//NS
			ways[1][2]		//ES
			) {
				return false;
			}
	} 
	ways[origin][destination]++;
	return true;
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
	//kprintf("origin %d, destination %d\n", origin, destination);
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
	lock_acquire(intersectionLock);
	ways[origin][destination]--;
	cv_signal(intersectionCv, intersectionLock);
	lock_release(intersectionLock);
}
