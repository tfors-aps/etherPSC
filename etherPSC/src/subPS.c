/*                                Copyright 1996
**                                      by
**                         The Board of Trustees of the
**                       Leland Stanford Junior University.
**                              All rights reserved.
**
**
**         Work supported by the U.S. Department of Energy under contract
**       DE-AC03-76SF00515.
**
**                               Disclaimer Notice
**
**        The items furnished herewith were developed under the sponsorship
**   of the U.S. Government.  Neither the U.S., nor the U.S. D.O.E., nor the
**   Leland Stanford Junior University, nor their employees, makes any war-
**   ranty, express or implied, or assumes any liability or responsibility
**   for accuracy, completeness or usefulness of any information, apparatus,
**   product or process disclosed, or represents that its use will not in-
**   fringe privately-owned rights.  Mention of any product, its manufactur-
**   er, or suppliers shall not, nor is it intended to, imply approval, dis-
**   approval, or fitness for any particular use.  The U.S. and the Univer-
**   sity at all times retain the right to use and disseminate the furnished
**   items for any purpose whatsoever.                       Notice 91 02 01
*/
/*=============================================================================

  Abs:  PS Calculations.

  Name: subPS.c      
	   subPSRegister   - Register All Subroutines
           subPSAlarmDelta - Alarm   Limit Delta and Absolute Limits
           subPSWarnDelta  - Warning Limit Delta

  Proto: not required and not used.  All functions called by the
         subroutine record get passed one argument:

         psub                       Pointer to the subroutine record data.
          Use:  pointer               
          Type: struct subRecord *    
          Acc:  read/write            
          Mech: reference

         All functions return a long integer.  0 = OK, -1 = ERROR.
         The subroutine record ignores the status returned by the Init
         routines.  For the calculation routines, the record status (STAT) is
         set to SOFT_ALARM (unless it is already set to LINK_ALARM due to
         severity maximization) and the severity (SEVR) is set to psub->brsv
         (BRSV - set by the user in the database though it is expected to
          be invalid).

  Auth: 08-Jan-2004, Stephanie Allison

=============================================================================*/

#include <subRecord.h>        /* for struct subRecord      */
#include <registryFunction.h> /* for epicsRegisterFunction */
#include <epicsExport.h>      /* for epicsRegisterFunction */

long subPSAlarmDelta(struct subRecord *psub)
{
  /*
   * Calculate alarm limits from desired deltas and reference.
   *
   * Inputs:
   *          A = Reference
   *          B = Delta for Alarm   Limits (HIHI, LOLO)
   *          C = Delta for Warning Limits (HIGH, LOW)
   * Outputs:
   *          I = HIHI value
   *          J = HIGH value
   *          K = LOW  value
   *          L = LOLO value
   *        VAL = Delta for Alarm Limits
   */
  psub->val = psub->b;
  psub->i = psub->a + psub->b;
  psub->l = psub->a - psub->b;
  psub->j = psub->a + psub->c;
  psub->k = psub->a - psub->c;
  return 0;
}

long subPSWarnDelta(struct subRecord *psub)
{
  /*
   * Calculate warning and alarm deltas from gold value or setpoint.
   * If the alarms are disabled or the setpoint is currently changing,
   * then set deltas to large values to disable alarms.
   *
   * Inputs:
   *          A = Current Setpoint
   *          B = Setpoint above which % is used and
   *              below which delta is used (must be positive)
   *          C = Gold    Setpoint
   *          D = Warning Delta
   *          E = Multiplier for Alarm Limits (must be > 1)
   *          F = Setpoint currently changing? (0=no, 1=yes)
   *              (0=calc limits, 1=disable limits)
   *          G = State (0=off, 1=on)
   *              (1=calc limits, 0=disable limits)
   *          H = Limits State
   *              (0=disable limits, 1=use gold, 2=use setpoint)
   *          I = Warning Fraction
   * Outputs:
   *          K = Reference (gold or setpoint)
   *          L = Delta for Alarm   Limits
   *        VAL = Delta for Warning Limits
   */
  /* If setpoint is changing or the power supply is off,
     set deltas to large numbers to disabling alarming. */
  if ((psub->f > 0.5) || (psub->g < 0.5) || (psub->h < 0.5)) {
    psub->val = 1000000.0;
    psub->l   = 1000000.0;
    psub->k   = 0.0;
  } else {
    /* Set reference value based on limits state. */
    if (psub->h < 1.5) psub->k = psub->c;
    else               psub->k = psub->a;
    /* Calculate warning limits tolerance.  Use either fraction or
       absolute value depending on current setpoint value. */
    if (psub->b < 0) psub->b = -psub->b;
    if (((psub->k >= 0.0) && (psub->k >  psub->b)) ||
        ((psub->k <  0.0) && (psub->k < -psub->b))) {
      psub->val = psub->k * psub->i;
    } else {
      psub->val = psub->d;
    }
    /* Calculate alarm limits tolerance using a multiplier. */
    psub->l = psub->val * psub->e;
    /* Tolerances must be positive. */
    if (psub->val < 0.0) psub->val = -psub->val;
    if (psub->l   < 0.0) psub->l   = -psub->l;
  }
  return 0;
}

epicsRegisterFunction(subPSAlarmDelta);
epicsRegisterFunction(subPSWarnDelta);
