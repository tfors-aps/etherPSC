program psControl("ps=00G-QD1,limit=DRVH,vi=Curr,vis=I,turnonmin=0")

/*
======================================================================
 
  Abs:  Sequence to control (standardize or ramp) a power supply.

	When user requests a standardization, the following is done
        for a specified number of cycles:
	- set current to maximum and wait.
	- set current to minimum and wait.
	At the end, set the current to the config value and wait for 
	the ramp to finish.

	After current is set to the minimum or maximum value, always wait for 
        the ramp to finish before waiting further for the specified time.  
        The standardization is automatically aborted if the power supply is 
        switched off (due to either an emergency off or a fault) or if 
	ramping takes too long.  The operator may abort the standardization 
	at any time.  If aborted, the current is set to 
        zero.  A status record is updated as the action progresses.
 
	When user requests a ramp-off, the following is done:
	- set current to zero and wait for ramp to finish.
	- turn off the power supply.

        The ramp-off is automatically aborted if the power supply is 
        switched off (due to either an emergency off or a fault).  If 
        ramping takes too long, the ramp is aborted and the power supply
	is turned off.  A status record is updated as the action progresses.
	A ramp-off cannot be aborted. 

	When user requests a ramp-on and the power supply is off, 
	the following is done:
	- if turnonmin is not zero, set the current to minimum.
	- the power supply is turned on
	- wait for the power supply to go on
	- retry turn-on a few times (to wait for any fault to clear).
	- if the power supply still does not go on, abort.
	- set the current to desired.

	When user requests a setpoint change, the following is done:
	- wait for ramp to start.
	- wait for ramp to finish.
	- force processing of alarm limits based on the new setpoint

  Name: psControl.st

  Auth: 12-Jan-2004, Stephanie Allison

======================================================================
*/

option +r;  /* one sequence task per power supply                   */
option -a;  /* All pvGets must be synchronous                       */
option +c;  /* All connections must be made before execution begins */

%%#include <epicsPrint.h>  /* epicsPrintf prototype */
%%#include <string.h>      /* strncmp     prototype */
#include "psControl.h"     /* shared PVs and defines with tspControl.st */

#define RAMP_MAX_DELAY 30.0  /* sec */
#define MIN_CURRENT    0.1   /* A   */
#define MAX_ON_REQUEST 2
#define ON_WAIT_DELAY  5     /* sec */
#define STEP_MIN_DELAY 0.02  /* sec */   

/* Standardization PVs - per power supply */

int     standardize;
assign  standardize      to "{ps}:Standardize";
monitor standardize;

int     cycle_count;
assign  cycle_count      to "{ps}:StandardizeNumCycle";

int     max_cycle_count;
assign  max_cycle_count  to "{ps}:StandardizeNumCycleMax";
monitor max_cycle_count;

double  ps_curr_drvh;
assign  ps_curr_drvh     to "{ps}:{vi}Setpt.DRVH";
monitor ps_curr_drvh;

double  ps_curr_drvl;
assign  ps_curr_drvl     to "{ps}:{vi}Setpt.DRVL";
monitor ps_curr_drvl;

double  ps_curr_min;
assign  ps_curr_min      to "{ps}:{vi}SetptMin";
monitor ps_curr_min;

double  ps_curr_res;
assign  ps_curr_res      to "{ps}:{vi}SetptConf";
monitor ps_curr_res;
evflag  ps_curr_res_ef;
sync    ps_curr_res ps_curr_res_ef;

double  ps_didt;
assign  ps_didt          to "{ps}:{vi}D{vis}Dt";
monitor ps_didt;

double  ps_rampdelay;
assign  ps_rampdelay     to "{ps}:RampStepDelay";
monitor ps_rampdelay;

/* Local Variables */

char*	ps_c;
char*	macro_c;
int	use_drv;
int	use_min;
int	pos_pol;
int	setpt_change;
int	zero_current;
int	des_current;
int	ramp_current;
int	wait_needed;
int	ramp_timeout;
int	change_count;
int	standardizing;
int 	ramping;
double	wait_delay_increment;
double	wait_delay_total;
double	sw_ramp_delay;
double	ps_curr_setpt_new;
double	ps_curr_setpt_prev;

ss psControl
{
/*
**  Initialization.
*/
    state psControl_init
    {
	when()
	{
	    ps_c = macValueGet("ps");
	    macro_c = macValueGet("limit");
	    pos_pol = 1;
	    if (macro_c && (strncmp(macro_c, "DRV", 3))) {
		use_drv = 0;
	    } else {
		use_drv = 1; 
		if (macro_c && (strncmp(macro_c, "DRVH", 4))) 
		   pos_pol = 0;
	    } 
	    macro_c = macValueGet("turnonmin");
	    if (macro_c && (strcmp(macro_c, "0"))) use_min = 1;
	    else                                   use_min = 0;
	    ps_status = STATUS_DONE;
	    pvPut(ps_status);
	    cycle_count  = 0;
	    pvPut(cycle_count);
	    wait_delay_sofar = 0;
	    pvPut(wait_delay_sofar);
	    standardize = 0;
	    pvPut(standardize);
	    norampoff = 1;
	    pvPut(norampoff);
	    control_setpt = CONTROL_DONE;
	    pvPut(control_setpt);
	    efClear(ps_curr_setpt_ef);
	    efClear(ps_curr_res_ef);
	    ps_limits_flag = 0;
	    pvPut(ps_limits_flag);
	    ps_curr_setpt_prev = ps_curr_setpt;

	} state psControl_request
    }
/*
**  Wait for the next request to perform an action.
*/
    state psControl_request
    {
	entry
	{
	    setpt_change  = 0;
	    ramping       = 0;
	    standardizing = 0;
	    cycle_count   = 0;
	    change_count  = 0;
	    wait_needed   = 0;
	    ramp_timeout  = 0;
	    zero_current  = 0;
	    des_current   = 0;
	    ramp_current  = 0;
	}
/*
**      Allow turn-on-and-ramp-to-des when power supply is off.
*/
	when((!ps_on) && (control_setpt == RAMP_ON))
	{
	    epicsPrintf("Ramp on started for %s\n", ps_c);
	    pvPut(cycle_count);
	    ps_status = STATUS_WAITING;
	    pvPut(ps_status);
	    if (use_min) ps_curr_setpt = ps_curr_min;
	    else         ps_curr_setpt = 0.0;
	    pvPut(ps_curr_setpt);
	    ps_curr_setpt_prev = ps_curr_setpt;

	} state psControl_on_wait
/*
**      Ignore all other actions when power supply is off.
*/
	when((!ps_on) && ((!norampoff) || standardize ||
			  efTest(ps_curr_setpt_ef) ||
			  efTest(ps_curr_res_ef)  ||
			  ((control_setpt != RAMP_ON) && 
			   (control_setpt != CONTROL_DONE))))
	{
	    if ((control_setpt == RAMP_ZERO) || 
		(control_setpt == RAMP_OFF)) {
		ps_curr_setpt = 0.0;
		pvPut(ps_curr_setpt);
	    }
	    ps_status = STATUS_DONE;
	    pvPut(ps_status);
	    norampoff = 1;
	    pvPut(norampoff);
	    standardize = 0;
	    pvPut(standardize);
	    control_setpt = CONTROL_DONE;
	    pvPut(control_setpt);
	    efClear(ps_curr_setpt_ef);
	    efClear(ps_curr_res_ef);
	    ps_curr_setpt_prev = 0.0;

	} state psControl_request
/*
**      Ignore an invalid request.
*/
	when((control_setpt < 0) || (control_setpt > CONTROL_MAX)) {
	    control_setpt = CONTROL_DONE;
	    pvPut(control_setpt);
	} state psControl_request
/*
**      Process all user requests that require the power supply be on.
*/
	when((!norampoff) || standardize || (control_setpt != CONTROL_DONE) || 
	     efTest(ps_curr_res_ef))
	{
	    ps_limits_flag = 1;
	    pvPut(ps_limits_flag);
	    ps_curr_setpt_prev = ps_curr_setpt;
	    if ((control_setpt == STANDARDIZE) || standardize) {
		ps_didt_select = 0;
		pvPut(ps_didt_select);
		zero_current  = 1;
		standardize = 1;
		pvPut(standardize);
		control_setpt = STANDARDIZE;
		pvPut(control_setpt);
		standardizing = 1;
		epicsPrintf("Standardize started for %s\n", ps_c);
	    }
	    else if ((!norampoff) || (control_setpt == RAMP_OFF)) {
		ps_didt_select = 0;
		pvPut(ps_didt_select);
		norampoff = 0;
		pvPut(norampoff);
		control_setpt = RAMP_OFF;
		pvPut(control_setpt);
		ramping = 1;
		epicsPrintf("Ramp off started for %s\n", ps_c);
	    } else if (efTest(ps_curr_res_ef)) {
		efClear(ps_curr_res_ef);
		control_setpt = RAMP_RESTORE;
		pvPut(control_setpt);
		ramping = 1;
	    } else if (control_setpt == RAMP_RESTORE) {
		pvGet(ps_curr_res);
		ramping = 1;
	    } else {
		ramping = 1;
	    }
	    pvPut(cycle_count);
            if (ps_rampdelay >= 0) {
              ps_ramp = 0;
              pvPut(ps_ramp);
            }

	} state psControl_ramp_wait

	when(efTest(ps_curr_setpt_ef))
	{
	    efClear(ps_curr_setpt_ef);
	    setpt_change   = 1;
	    wait_needed    = 1;
	    cycle_count    = max_cycle_count + 1;
	    ps_limits_flag = 1;
	    pvPut(ps_limits_flag);
            if (ps_rampdelay >= 0) {
		ps_ramp = 1;
		pvPut(ps_ramp);
		sw_ramp_delay = 0;
            } else if (ps_curr_setpt != ps_curr_setpt_prev) {
		sw_ramp_delay = RAMP_DELAY;
	    } else {
		sw_ramp_delay = 0;
	    }
	} state psControl_ramp_start
    }
/*
**  Wait for turn-on to finish.  Check for an abort while we wait.
*/
    state psControl_on_wait
    {
	entry
	{
	    ps_on_des = 1;
	    pvPut(ps_on_des);
        }
	when(control_setpt != RAMP_ON)
	{
	} state psControl_request

	when(delay(ON_WAIT_DELAY))
	{
	} state psControl_on_check
    }
/*
**  Wait for turn-on to finish.  Ramp to des when done.
**  Retry on request a few times if necessary.
*/
    state psControl_on_check
    {
	when(ps_on)
	{
	    epicsPrintf("PS on - setting to des for %s\n", ps_c);

	} state psControl_request

	when(cycle_count > MAX_ON_REQUEST)
	{
	    epicsPrintf("Supply off - Ramp on aborted for %s\n", ps_c);
	    ps_status = STATUS_PS_OFF;
	    pvPut(ps_status);
	    control_setpt = CONTROL_DONE;
	    pvPut(control_setpt);

	} state psControl_request

	when()
	{
	    cycle_count++;
	    pvPut(cycle_count);

	} state psControl_on_wait
    }
/*
**  Wait for ramping to finish.
*/
    state psControl_ramp_wait
    {
	entry
	{
	    if (ps_ramp && (!setpt_change)) {
		ps_status = STATUS_RAMPING;
		pvPut(ps_status);
	    }
/*
**  	    If we are stepping, don't delay the first time since we need to
**	    make the first step.
*/
	    if ((ps_rampdelay >= 0) && ps_ramp) {
		if ((ps_rampdelay >= STEP_MIN_DELAY) && (!setpt_change) && 
		     (ps_curr_setpt_new != ps_curr_setpt)) {
		    sw_ramp_delay = 0;
/*
**  	        Otherwise, estimate time to finish ramping PS when not 
**		stepping.
*/
		} else {
		    if (ps_didt > 0) {
			sw_ramp_delay = (ps_curr_setpt - ps_curr_setpt_prev)/
					ps_didt;
		    	if (sw_ramp_delay < 0.0) sw_ramp_delay *= -1;
		    } else {
			sw_ramp_delay = ps_rampdelay;
		    }
		}
	    } else {
		sw_ramp_delay = RAMP_MAX_DELAY;
	    }
        }
	when(!ps_on)
	{
	} state psControl_done

	when(!ps_ramp)
	{
	} state psControl_wait

	when((ps_rampdelay >= 0) && delay(sw_ramp_delay))
	{
/*
**  	    For a PS with ramp-with-stepping enabled, ramp to new setpoint
**	    using dI/dt provided by the database.
*/
	    if ((!setpt_change) && (ps_rampdelay >= STEP_MIN_DELAY) && 
		(ps_curr_setpt != ps_curr_setpt_new)) {
		sw_ramp_delay = ps_rampdelay;
		if (ps_didt <= 0) {
		    ps_curr_setpt = ps_curr_setpt_new;
		} else if (ps_curr_setpt_new > ps_curr_setpt) {
		    ps_curr_setpt += ps_didt * ps_rampdelay;
		    if (ps_curr_setpt >= ps_curr_setpt_new)
			ps_curr_setpt  = ps_curr_setpt_new;
		} else {
		    ps_curr_setpt -= ps_didt * ps_rampdelay;
		    if (ps_curr_setpt <= ps_curr_setpt_new) 
			ps_curr_setpt  = ps_curr_setpt_new;
		}
		pvPut(ps_curr_setpt);
	    } else {
		ps_ramp = 0;
		pvPut(ps_ramp);
	    }
	} state psControl_ramp_wait

	when(delay(RAMP_MAX_DELAY))
	{
	    ramp_timeout = 1;
	    efClear(ps_curr_setpt_ef);

	} state psControl_done
    }
/*
** Wait for a specified amount of time before going to the next setting.
** Check for aborts and ramping while we wait.  Also, increment a timer 
** for users to watch.
*/
    state psControl_wait
    {
	entry
	{
	    wait_delay_increment = WAIT_DELAY;
	    wait_delay_sofar = 0;
	    pvPut(wait_delay_sofar);
	    wait_delay_total = wait_delay;
	    efClear(ps_curr_setpt_ef);
        }

	when((!ps_on) || (standardizing && ((!standardize) || (!norampoff) ||
					    (control_setpt != STANDARDIZE))) ||
	     (setpt_change && (standardize || (!norampoff) || 
			       (control_setpt != CONTROL_DONE))) ||
	     ((cycle_count >  max_cycle_count) && 
              ((wait_delay_sofar >= wait_delay_total) || 
	       (control_setpt == RAMP_OFF))))
	{
	} state psControl_done

	when(ps_ramp)
	{
            if (ps_rampdelay >= 0) {
		ps_ramp = 0;
		pvPut(ps_ramp);
	    }
	} state psControl_ramp_wait

	when ((!wait_needed) || ((cycle_count <= max_cycle_count) &&
				 (wait_delay_sofar >= wait_delay)))
	{
	} state psControl_set_current

	when (delay(wait_delay_increment))
	{
	    if (!setpt_change) {
		ps_status = STATUS_WAITING;
		pvPut(ps_status);
	    }
	    wait_delay_sofar += wait_delay_increment;
	    pvPut(wait_delay_sofar);
	    wait_delay_increment = wait_delay_total - wait_delay_sofar;
	    if (wait_delay_increment > WAIT_DELAY) 
		wait_delay_increment = WAIT_DELAY;

	} state psControl_wait
    }
/*
** Set the power supply to its next setpoint.
*/
    state psControl_set_current
    {
	when() {
	    wait_needed = 1;
	    ps_curr_setpt_prev = ps_curr_setpt;
	    if (ramping)
	    {
		cycle_count = max_cycle_count + 1;
		if ((control_setpt == RAMP_DES) || 
		    (control_setpt == RAMP_ON)) {
		    ps_curr_setpt = ps_curr_des;
		    des_current   = 1;
		} else if (control_setpt == RAMP_MIN) {
		    ps_curr_setpt = ps_curr_min;
		    ramp_current  = 1;
		} else if (control_setpt == RAMP_RESTORE) {
		    ps_curr_setpt = ps_curr_res;
		    ramp_current  = 1;
		} else {
		    ps_curr_setpt = 0.0;
		    zero_current  = 1;
		}
	    }
	    else if (cycle_count < max_cycle_count) 
	    {
		change_count++;
		cycle_count = change_count/2;
		pvPut(cycle_count);
		zero_current = zero_current ? 0 : 1;
		if (zero_current)  ps_curr_setpt = ps_curr_min;
		else if (use_drv || 
			 ((ps_curr_des < MIN_CURRENT) &&
			  (ps_curr_des > -MIN_CURRENT)))
		    if (pos_pol)   ps_curr_setpt = ps_curr_drvh;
		    else           ps_curr_setpt = ps_curr_drvl;
		else               ps_curr_setpt = ps_curr_des;
	    } 
	    else
	    {
		cycle_count   = max_cycle_count + 1;
		ps_curr_setpt = ps_curr_des;
	    }
	    ps_curr_setpt_new = ps_curr_setpt;
            if (ps_rampdelay >= 0) {
		ps_ramp = 1;
		pvPut(ps_ramp);
		sw_ramp_delay = 0;
            } else if (ps_curr_setpt != ps_curr_setpt_prev) {
		sw_ramp_delay = RAMP_DELAY;
	    } else {
		sw_ramp_delay = 0;
	    }
	    if ((ps_rampdelay < STEP_MIN_DELAY) || (ps_didt <= 0)) {
		pvPut(ps_curr_setpt);
	    } else {
		ps_curr_setpt = ps_curr_setpt_prev;
	    }
	} state psControl_ramp_start
    }
/*
**  Wait for ramp to start.
*/
    state psControl_ramp_start
    {
	when(!ps_on)
	{
	} state psControl_done

	when(ps_ramp || delay(sw_ramp_delay))
	{
	} state psControl_ramp_wait
    }
/*
**  Done - if aborted, take action depending on request, log a message, and
**  update status.
*/
    state psControl_done
    {
	when()
	{
	    ps_status = STATUS_DONE;
	    if (standardizing) {
		if (!ps_on) {
		    epicsPrintf("Supply off - Standardize aborted for %s\n", ps_c);
		    ps_status = STATUS_PS_OFF;
		}
		else if ((!standardize) || (!norampoff) || 
		         (control_setpt != STANDARDIZE)) {
		    epicsPrintf("Standardize aborted by user for %s\n", ps_c);
		    ps_status = STATUS_ABORT;
		}
		else if (ramp_timeout) {
		    epicsPrintf("Ramp timeout - Standardize aborted for %s\n", ps_c);
		    ps_status = STATUS_RAMP_TIMEOUT;
		}
		else {
		    epicsPrintf("Standardize completed for %s\n", ps_c);
		}
		standardize = 0;
		pvPut(standardize);
		if (control_setpt == STANDARDIZE) {
		    control_setpt = CONTROL_DONE;
		    pvPut(control_setpt);
		}
		if (ps_status != STATUS_DONE) {
		    if (ps_curr_setpt != 0.0) {
			ps_curr_setpt = 0.0;	
			pvPut(ps_curr_setpt);
			efSet(ps_curr_setpt_ef);
		    }
		}
	    } else if (zero_current || ramp_timeout || (!ps_on)) {
 		if ((!norampoff) || (control_setpt == RAMP_OFF)) {
		    epicsPrintf("PS ramped to zero - setting PS off for %s\n", ps_c);
		    efClear(ps_curr_setpt_ef);
		    norampoff = 1;
		    pvPut(norampoff);
		    ps_on_des = 0;
		    pvPut(ps_on_des);
		}
		if (ramp_timeout || (!ps_on) || 
		    (control_setpt == RAMP_OFF) || 
		    (control_setpt == RAMP_ZERO)) {
		    control_setpt = CONTROL_DONE;
		    pvPut(control_setpt);
		}
	    } else if ((!setpt_change) && 
		       ((des_current &&
			 ((control_setpt == RAMP_ON) || 
			  (control_setpt == RAMP_DES))) ||
		        (ramp_current && 
			 ((control_setpt == RAMP_MIN) ||
			  (control_setpt == RAMP_RESTORE))))) {
		control_setpt = CONTROL_DONE;
		pvPut(control_setpt);
	    }
	    pvPut(ps_status);
	    if (!efTest(ps_curr_setpt_ef) || (!ps_on)) {
		ps_limits_flag = 0;
		pvPut(ps_limits_flag);
	    }
            if ((ps_rampdelay >= 0) && ps_ramp) {
              ps_ramp = 0;
              pvPut(ps_ramp);
            }
	    ps_curr_setpt_prev = ps_curr_setpt;

	} state psControl_request
    }
}

exit{}
