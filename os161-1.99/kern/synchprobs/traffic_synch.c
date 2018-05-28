#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

static int ns = 0;
static int ne = 0;
static int nw = 0;
static int sn = 0;
static int sw = 0;
static int se = 0;
static int ew = 0;
static int en = 0;
static int es = 0;
static int we = 0;
static int wn = 0;
static int ws = 0;
static bool requestStop = false;
static Direction curOrigin;
static Direction curDestination;

bool checkIntersection(Direction origin, Direction destination);
void decrement(Direction origin, Direction destination);

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
static struct lock* intersectionLock;
static struct cv* intersectionCV;
static struct cv* firstWaitingCV;
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
  /* replace this default implementation with your own implementation */

  intersectionLock = lock_create("intersectionLock");
  intersectionCV = cv_create("intersectionCV");
  firstWaitingCV = cv_create("firstWaitingCV");

  if (intersectionLock == NULL) {
    panic("could not create intersection semaphore");
  } else if (intersectionCV == NULL) {
    panic("could not create intersection cv");
  } else if (firstWaitingCV == NULL) {
    panic("could not create firstWaiting cv");
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
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionCV != NULL);
  KASSERT(firstWaitingCV != NULL);
  KASSERT(intersectionLock != NULL);

  lock_destroy(intersectionLock);
  cv_destroy(intersectionCV);
  cv_destroy(firstWaitingCV);
}

// Check if it will collide with other car that is at the intersection
bool checkIntersection(Direction origin, Direction destination) {
  if (origin == north) {  // North
    if (destination == south
      && !ew && !we && !sw && !wn && !ws && !es) {  // North South
      ++ns;
    } else if (destination == east
      && !sn && !sw && !se && !we && !wn && !es && !ew) {  // North East
      ++ne;
    } else if (destination == west && !sw && !ew) {   // North West
      ++nw;
    } else {
      return false;
    }
  } else if (origin == south) {  // South
    if (destination == north
      && !ne && !we && !wn && !ew && !es && !en) {  // South North
      ++sn;
    } else if (destination == east && !ne && !we) {    // South East
      ++se;
    } else if (destination == west
      && !ns && !nw && !ne && !we && !wn && !ew && !es) {  // South West
      ++sw;
    } else {
      return false;
    }
  } else if (origin == east) {   // East
    if (destination == west
      && !sn && !sw && !ns && !nw && !ne && !wn) {  // East West
      ++ew;
    } else if (destination == north && !sn && !wn) {      // East North
      ++en;
    } else if (destination == south
      && !sn && !sw && !ns && !ne && !we && !ws && !wn) { // East South
      ++es;
    } else {
      return false;
    }
  } else if (origin == west) { // West
    if (destination == east
      && !sn && !se && !sw && !ns && !ne && !es) { // West East
      ++we;
    } else if (destination == north
      && !sn && !sw && !ns && !ne && !ew && !en && !es) {  // West North
      ++wn;
    } else if (destination == south && !ns && !es) {  // West South
      ++ws;
    } else {
      return false;
    }
  }
  return true;
}

// Decrement the number of cars in the given direction
void decrement(Direction origin, Direction destination) {
  if (origin == north && destination == south) {
    --ns;
  } else if (origin == north && destination == east) {
    --ne;
  } else if (origin == north && destination == west) {
    --nw;
  } else if (origin == south && destination == north) {
    --sn;
  } else if (origin == south && destination == east) {
    --se;
  } else if (origin == south && destination == west) {
    --sw;
  } else if (origin == east && destination == west) {
    --ew;
  } else if (origin == east && destination == north) {
    --en;
  } else if (origin == east && destination == south) {
    --es;
  } else if (origin == west && destination == east) {
    --we;
  } else if (origin == west && destination == north) {
    --wn;
  } else if (origin == west && destination == south) {
    --ws;
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
  /* replace this default implementation with your own implementation */
  KASSERT(intersectionCV != NULL);
  KASSERT(firstWaitingCV != NULL);
  KASSERT(intersectionLock != NULL);

  lock_acquire(intersectionLock);
  while ((requestStop && origin != curOrigin && destination != curDestination)
      || !checkIntersection(origin, destination)) {
    if (requestStop == false) {
      requestStop = true;
      cv_wait(firstWaitingCV, intersectionLock);
    } else {
      cv_wait(intersectionCV, intersectionLock);
    }
  }
  curOrigin = origin;
  curDestination = destination;
  lock_release(intersectionLock);
}

/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersction.
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
  KASSERT(intersectionCV != NULL);
  KASSERT(firstWaitingCV != NULL);
  KASSERT(intersectionLock != NULL);

  lock_acquire(intersectionLock);
  decrement(origin, destination);
  requestStop = false;
  cv_signal(firstWaitingCV, intersectionLock);
  cv_broadcast(intersectionCV, intersectionLock);
  lock_release(intersectionLock);
}
