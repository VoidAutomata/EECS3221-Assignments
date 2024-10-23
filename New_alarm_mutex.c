/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link; //link to next alarm in list
    int                 alarm_id; // unique alarm ID to identify and edit
    int 				seconds; // will use this to measure time
    char* 				type; // type in string format, does not include T at start
    time_t              time;   /* seconds from EPOCH */
    char                message[64];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

//____ THREADS ____

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
        alarm = alarm_list;

        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0.
         */
        if (alarm == NULL)
            sleep_time = 1;
        else {
            alarm_list = alarm->link;
            now = time (NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                sleep_time, alarm->message);
#endif
            }

        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
        if (sleep_time > 0)
            sleep (sleep_time);
        else
            sched_yield ();

        /*
         * If a timer expired, print the message and free the
         * structure.
         */
        if (alarm != NULL) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

//____ FUNCTIONS ____

// Function to trim leading and trailing whitespace
// Code provided by Adam Rosenfield and Dave Gray on StackOverflow
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

// Function is called when user enters command for Start_Alarm
// Creates new alarm based on inputs, and then adds to alarm list
void Start_Alarm (int alarm_id, char* type, int seconds, const char* message){
    alarm_t *alarm, **last, *next; // pointers for alarms in list
    int status; // for checking mutex status
    printf("Starting Alarm %d\n", alarm_id);
    
    // Malloc for new alarm 
    // MUST FREE MEMORY ONCE ALARM EXPIRES
    alarm = (alarm_t*)malloc(sizeof(alarm_t));
    if (alarm == NULL){
		free (alarm);
        perror("Allocate Alarm");
    }
    alarm->alarm_id = alarm_id;
    alarm->type = type;
    alarm->seconds = seconds;
    alarm->time = time(NULL) + seconds; // current time + seconds
    strcpy(alarm->message, message); // copy string into the struct
    alarm->link = NULL;

    // lock mutex before operation
    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Lock Mutex");
    }
    
    last = &alarm_list;
    next = *last;

    while(next!= NULL){
        // compare items in list until current alarm_id is greater than last
        if (next->alarm_id >= alarm_id){
            alarm->link = next; //position found. Link alarm
            break;
        }
        last = &next->link; // move to next position in list
        next = next->link;
    }
    alarm_list = alarm; // insert new alarm at found position

    //printf("Successfully added alarm %d to list.\n", alarm->alarm_id);

    // unlock mutex after operation
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Unlock Mutex");
    }
    
}
void Change_Alarm (int alarm_id, char* type, int seconds, const char* message){
        alarm_t *alarm; // pointer for new alarm
        int status; // for checking mutex status
		printf("Changing alarm %d to T%s, %d, %s\n", alarm_id, type, seconds, message);

        // lock mutex
        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0){
            err_abort (status, "Lock Mutex");
        }

        // search for matching alarm based on alarm_id
        alarm = alarm_list;
        while (alarm != NULL){
            if (alarm->alarm_id == alarm_id){
                // found the alarm. Update fields
                strcpy(alarm->type, type);
                alarm->type[sizeof(alarm->type)-1] = '\0'; // set last value as terminator
                alarm->seconds = seconds;
                alarm->time = time(NULL) + seconds; // based on current time
                strcpy(alarm->message, message);
                alarm->message[sizeof(alarm->message)-1] = '\0'; // terminate string

                //printf("Alarm %d has been changed to T%s %d %s.\n", alarm_id, type, seconds, message);
                break;
            }
            // didn't find. move to next in list
            alarm = alarm->link;
        }

        if (alarm == NULL){
            // this alarm_id doesn't exist in the list
            printf("Could not find alarm %d\n", alarm_id);
        }

        // unlock mutex
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0){
            err_abort (status, "Unlock Mutex");
        }
	}

void Cancel_Alarm (int alarm_id){
    alarm_t *alarm, *prev;
    int status;

    printf("Canceling alarm %d\n", alarm_id);
    //free(alarm);

    // lock mutex
    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Lock Mutex");
    }

    // search for alarm in alarm_list to cancel
    prev = NULL;
    alarm = alarm_list;

    while (alarm != NULL){
        if (alarm->alarm_id == alarm_id){
            // found alarm. remove from list
            if (prev == NULL){
                // alarm is the first in list. make link the new head
                alarm_list = alarm->link;
            } else {
                prev->link = alarm->link;
            }
            // free memory
            free(alarm);
            break;
        }
        // move to next alarm in list
        prev = alarm;
        alarm = alarm->link;
    }

    if (alarm == NULL){
        // alarm to find does not exist
        printf("Alarm %d does not exist.\n", alarm_id);
    }

    // unlock mutex
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Unlock Mutex");
    }    
}

void View_Alarms(){
    alarm_t *alarm;
    int status;
    time_t now;
    int time_left;

    printf("Viewing Alarms\n");
    
    // lock mutex
    status = pthread_mutex_lock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Lock Mutex");
    }

    // get current time. needed when time changes
    now = time(NULL);

    // check alarm list
    if (alarm_list == NULL){
        printf("There are no alarms.\n");
    } else {
        // go through each alarm in list
        for (alarm = alarm_list; alarm != NULL; alarm = alarm->link){
            time_left = (int)(alarm->time - now);
            printf("Alarm(%d): T%s %d time left: %d seconds. Message: %s",
            alarm->alarm_id, alarm->type, alarm->seconds, time_left, alarm->message);
        }
    }

    // unlock mutex
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0){
        err_abort (status, "Unlock Mutex");
    }
}

// Below is the main function/thread
int main (int argc, char *argv[])
{
    int status;
    char sline[128];
    char line[128];
    alarm_t *alarm, **last, *next;
    int alarm_id; // declare the alarm's unique id
    char type[65] = "";
    int seconds; // time in seconds
    char message[65] = "";
    pthread_t thread;

    status = pthread_create (&thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    while (1) {
        printf ("alarm> ");
        if (fgets (sline, sizeof (sline), stdin) == NULL) exit (0);
        if (strlen (sline) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        // remove unnecessary white space around line (stdin)
        strcpy(line, trimwhitespace(sline));
        
        // check which function was called
        if (sscanf(line, "Start_Alarm(%d): T%s %d %128[^\n]", 
            &alarm_id, type, &seconds, message) > 3){
            Start_Alarm(alarm_id, type, seconds, message);
            printf("Alarm(%d) Inserted by Main Thread(<thread-id>) Into Alarm List at %d: T%s %d %s",
            alarm_id, (int)time(NULL), type, seconds, message);

            // Change Alarm function call
        } else if (sscanf(line, "Change_Alarm(%d): T%s %d %128[^\n]", 
            &alarm_id, type, &seconds, message) > 3){
            Change_Alarm(alarm_id, type, seconds, message);
            printf("Alarm(%d) Changed at %d: T%s %d %s",
            alarm_id, (int)time(NULL), type, seconds, message);

            // Cancel Alarm function call
        } else if (sscanf(line, "Cancel_Alarm(%d)", 
            &alarm_id) == 1){
            Cancel_Alarm(alarm_id);
            printf("Alarm(%d) Cancelled at %d: T%s %d %s",
            alarm_id, (int)time(NULL), type, seconds, message);

            // View Alarms function call. Uses specifically string compare, not sscanf
            // There are no variables to compare, only exact copy of a string
        } else if (strcmp(line, "View_Alarms()") == 0){
                View_Alarms();
        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        } else if (sscanf (line, "%d %64[^\n]", 
            &alarm->seconds, alarm->message) < 2) {
            printf("Bad command\n");
            //fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            alarm->time = time (NULL) + alarm->seconds;

            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */
             printf("else");
            last = &alarm_list;
            next = *last;
            while (next != NULL) {
				printf("while");
                if (next->time >= alarm->time) {
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link;
                next = next->link;
            }
            /*
             * If we reached the end of the list, insert the new
             * alarm there. ("next" is NULL, and "last" points
             * to the link field of the last item, or to the
             * list header).
             */
            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
#ifdef DEBUG
            printf ("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf ("%d(%d)[\"%s\"] ", next->time,
                    next->time - time (NULL), next->message);
            printf ("]\n");
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        }
    }
}
