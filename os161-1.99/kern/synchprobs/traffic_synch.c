#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

static int volatile NE = 0;
static int volatile NS = 0;
static int volatile NW = 0;
static int volatile ES = 0;
static int volatile EW = 0;
static int volatile EN = 0;
static int volatile SW = 0;
static int volatile SN = 0;
static int volatile SE = 0;
static int volatile WN = 0;
static int volatile WE = 0;
static int volatile WS = 0;

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
static struct semaphore *intersectionSem;
static struct lock *intersectionLock;
static struct cv *intersectionCv;

bool
hit_happen(int volatile check[])
{
	int checkLen = sizeof(check);
	for (int i = 0; i < checkLen; i++) {
		int volatile check_direction = check[i];
		if (check_direction != 0) {
			return true;
		}
	}
	return false;
}



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
	int volatile need_to_check[7];
	if (origin == 0) { //N
		switch (destination) {
		case 0: //NN
			//int volatile need_to_check[] = {};
			panic("input warning: from north to north");
			break;
		case 1: //NE
			//NE, NS, NW, EN, WS available
			int volatile need_to_check[7] = { ES,EW,SW,SN,SE,WN,WE };
			//need_to_check[0] = ES;
			//need_to_check[1] = EW;
			//need_to_check[2] = SW;
			//need_to_check[3] = SN;
			//need_to_check[4] = SE;
			//need_to_check[5] = WN;
			//need_to_check[6] = WE;

			if (hit_happen(need_to_check[7])) {
				return false;
			} else {
				NE++;
				break;
			}
		case 2: //NS
			//NE, NS, NW, SN, SE, EN available
			int volatile need_to_check[6] = {ES,EW,SW,WN,WE,WS};
			//need_to_check[0] = ES;
			//need_to_check[1] = EW;
			//need_to_check[2] = SW;
			//need_to_check[3] = WN;
			//need_to_check[4] = WE;
			//need_to_check[5] = WS;
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				NS++;
				break;
			}
		case 3: //NW
			//NE, NS, NW, WN, NOT DESTINATION WITH WEST available
			int volatile need_to_check[2] = {SW,EW};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				NW++;
				break;
			}
		}
	} else if (origin == 1) { //E
		switch (destination) {
		case 0: //EN
				//EN, ES, EW, NE, ALL OTHERS NOT HAVE NORTH AS DESTINATION  available
			int volatile need_to_check[2] = {SN,WN};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				EN++;
				break;
			}
		case 1: //EE
			panic("input warning: from east to east");
			break;

		case 2: //ES
				//EN, ES, EW, SE, NW available
			int volatile need_to_check [7]= {NE,NS,SW,SN,WN,WE,WS};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				ES++;
				break;
			}
		case 3: //EW
				//EN, ES, EW, WE, EN, WS, SE available
			int volatile need_to_check[6] = {NE,NS,NW,SW,SN,WN};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				EW++;
				break;
			}
		}

	} else if (origin == 2) { //S
		switch (destination) {
		case 0: //SN
				//SN, SE, SW, NS, NW, SW, WS available
			int volatile need_to_check[6] = {NE,ES,EW,EN,WN,WE};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				SN++;
				break;
			}
		case 1: //SE
				//SN, SE, SW, ES, EN, NW, ALL NOT EAST DESTINATION available
			int volatile need_to_check[2] = {WE,NE};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				SE++;
				break;
			}
		case 2: //SS
			panic("input warning: from south to south");
			break;

		case 3: //SW
				//SN, SE, SW, WS, EN, WS, SE available
			int volatile need_to_check[7] = {NE,NS,NW,ES,EW,WN,WE};
			if (hit_happen(need_to_check)) {
				return false;
			} else {
				SW++;
				break;
			}
		}

	} else { //W
		switch (destination) {
		case 0: //WN
				//WN, WS, WE, NW, WS, SE available
			int volatile need_to_check[7] = { NE,NS,ES,EW,EN,SW,SN };
			if (hit_happen(need_to_check)) {
				return false;
			}
			else {
				WN++;
				break;
			}
		case 1: //WE
				//WN, WS, WE, EW, EN, NW, WS available
			int volatile need_to_check[6] = { NE,NS,ES,SW,SN,SE };
			if (hit_happen(need_to_check)) {
				return false;
			}
			else {
				WE++;
				break;
			}
		case 2: //WS
				//WN, WS, WE, SW, NOT SOUTH DESTINATION available
			int volatile need_to_check[2] = { NS,ES };
			if (hit_happen(need_to_check)) {
				return false;
			}
			else {
				WS++;
				break;
			}
		case 3: //WW
			panic("input warning: from west to west");
			break;
		}
	}
	return true;
}

void
one_intersection_end(Direction origin, Direction destination) {
	if (origin == 0) { //N
		switch (destination)
		{
		case 0: //N
			panic("input warning: from north to north");
			break;
		case 1: //E
			NE--;
			break;
		case 2: //S
			NS--;
			break;
		case 3: //W
			NW--;
			break;
		}
	} else if (origin == 1) { //E
		switch (destination)
		{
		case 0: //N
			EN--;
			break;
		case 1: //E
			panic("input warning: from east to east");
			break;
		case 2: //S
			ES--;
			break;
		case 3: //W
			EW--;
			break;
		}
	} else if (origin == 2) { //S
		switch (destination)
		{
		case 0: //N
			SN--;
			break;
		case 1: //E
			SE--;
			break;
		case 2: //S
			panic("input warning: from south to south");
			break;
		case 3: //W
			SW--;
			break;
		}
	} else { //W
		switch (destination)
		{
		case 0: //N
			WN--;
			break;
		case 1: //E
			WE--;
			break;
		case 2: //S
			WS--;
			break;
		case 3: //W
			panic("input warning: from west to west");
			break;
		}
	}
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
	one_intersection_end(origin, destination);
	cv_signal(intersectionCv, intersectionLock);
	lock_release(intersectionLock);
}
