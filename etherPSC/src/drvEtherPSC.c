/* drvEtherPSC.c - Driver Support Routines SPEAR Ethernet Power Supply
 *		 CONTROLLER
 *
 *	Author:		Clemens Wermelskirchen
 *	Date:		08-OCT-2005
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"alarm.h"
#include	"dbCommon.h"
#include	"dbDefs.h"
#include	"dbScan.h"
#include	"dbLock.h"
#include	"drvSup.h"
#include	"devSup.h"
#include        "errMdef.h"        /* errMessage()         */
#include	"epicsThread.h"
#include	"errlog.h"
#include	<devLib.h>
#include	"aiRecord.h"
#include	"aoRecord.h"
#include	"biRecord.h"
#include	"longinRecord.h"
#include	"stringinRecord.h"
#include	"recSup.h"
#include	"link.h"
#include        "iocsh.h"
#include	"epicsExport.h"
#include	"initHooks.h"

#include	"etherPSCInclude.h"

#define	DEBUG		0

#ifndef OK
#define	OK	0
#endif
#ifndef ERROR
#define	ERROR	-1
#endif

/* Global variables */

double EtherPSCDelay = 0.300;  /* Seconds */

/* Local variables */

static ETHERPSC		etherpsc = { 0 };

typedef struct
	{
	    unsigned short	datalength;
	    unsigned short	port;
	    unsigned char	cmd;
	    unsigned char	rsp;
	    unsigned char	data[256];
	} ETHERPSCMSG;

static const ETHERPSCMSG  PS_ON_MSG = {
                4, 2000, BITBUSCMD_PS_ON, 0x00,
                { BITBUSCMD_PS_ON, 0 } };

static const ETHERPSCMSG  PS_OFF_MSG = {
                4, 2000, BITBUSCMD_PS_OFF, 0x00,
                { BITBUSCMD_PS_OFF, 0 } };

static const ETHERPSCMSG  RESET_PS_MSG = {
                4, 2000, BITBUSCMD_CNTL_RESET, 0x00,
                { 0, 1 } };

static const ETHERPSCMSG  PS_INT_RESET_MSG = {
                4, 2000, BITBUSCMD_INT_RESET, 0x00,
                { BITBUSCMD_INT_RESET, 0 } };

static const ETHERPSCMSG  PS_CURRENT_AC_MSG = {
                11, 2000, BITBUSCMD_SET_I, 0x00,
                { BITBUSCMD_SET_I, 1, 0, 0, 0, 0, 0, 1, 0 } };

/*                      this command is not being used
static const ETHERPSCMSG  SHORT_STATUS_MSG = {
                4, 2000, BITBUSCMD_SHORT_STATUS, 0x00,
                { BITBUSCMD_SHORT_STATUS, 0 } };
*/

static const ETHERPSCMSG  FAST_STATUS_MSG = {
                4, 2000, BITBUSCMD_FAST_STATUS, 0x00,
                { BITBUSCMD_FAST_STATUS, 0 } };

static const ETHERPSCMSG  INFO_MSG_MSG = {
                4, 2000, BITBUSCMD_INFO_MSG, 0x00,
                { BITBUSCMD_INFO_MSG, 0 } };

static const ETHERPSCMSG  DIAG_STATUS_MSG = {
                4, 2000, BITBUSCMD_DIAG_RDBK, 0x00,
                { BITBUSCMD_DIAG_RDBK, 0 } };

static const ETHERPSCMSG  DES_I_RDBK_MSG = {
                5, 2000, BITBUSCMD_DES_I_RDBK, 0x00,
                { BITBUSCMD_DES_I_RDBK, 1, 0 } };

static const ETHERPSCMSG  CNTL_INFO_MSG = {
                4, 2000, BITBUSCMD_CNTL_INFO, 0x00,
                { BITBUSCMD_CNTL_INFO, 0 } };

static const ETHERPSCMSG  CAL_DATA_MSG = {
                4, 2000, BITBUSCMD_CAL_DATA, 0x00,
                { BITBUSCMD_CAL_DATA, 0 } };

static const ETHERPSCMSG  ANALOG_RDBK_MSG = {
                4, 2000, BITBUSCMD_ANALOG_RDBK, 0x00,
                { BITBUSCMD_ANALOG_RDBK, 0 } };


/* local function prototypes */
static long init( );
static long report( int level );
static EPICSTHREADFUNC etherPSC_input_thread( ETHERPSC *net );
static EPICSTHREADFUNC etherPSC_output_thread( ETHERPSC *net );
static void process_etherpsc_rsp ( ETHERPSCNODE *node, unsigned char *rsp, long n );
static void process_record_si( ETHERPSCNODE*, int, unsigned char*, int );
static void process_record_li( ETHERPSCNODE*, int, long );
static void process_record_ao( ETHERPSCNODE *node, int signal, float f );


/* Global variables        */
/* Driver entry table */

struct {
	long		number;
	DRVSUPFUN	report;
	DRVSUPFUN	init;
} drvEtherPSC={
	2,
	report,
	init};
epicsExportAddress(drvet, drvEtherPSC);

/* Driver initialization routines */

static void initThreads(initHookState state)
{
  if (state == initHookAfterInitDatabase) {
    if ( ! epicsThreadCreate( "EtherPSC_rcv", epicsThreadPriorityMedium,
			epicsThreadGetStackSize(epicsThreadStackMedium),
			(EPICSTHREADFUNC) etherPSC_input_thread,
			(void*) &etherpsc ) ) {
	printf( "drvEtherPSC: receiver thread creation failed\n" );
	exit( 1 );
    }

    if ( ! epicsThreadCreate( "EtherPSC_snd", epicsThreadPriorityMedium,
                        epicsThreadGetStackSize(epicsThreadStackMedium),
                        (EPICSTHREADFUNC) etherPSC_output_thread,
                        (void*) &etherpsc ) ) {
        printf( "drvEtherPSC: transmission thread creation failed\n" );
        exit( 1 );
    }
  }
}
static long init()
{
    /* create socket for communication, for now, there is only one */

    if ( ( etherpsc.sock = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
	printf( "drvEtherPSC: failed to create socket: %s", strerror(errno) );
	exit( ERROR );
    }
    initHookRegister(initThreads);
    return(OK);
}


/*
 * Ether_io_report - report ethernet controllers present
 */
long Ether_io_report ( int level )
{
    ETHERPSCNODE	*pnode;

    printf( "\nEthernet & PSC Configuration\n" );

    for ( pnode = etherpsc.pnode; pnode; pnode = (ETHERPSCNODE*) pnode->pnode )
    {
	if ( pnode->present )
	{
	    printf( "  EtherPSC %15s alive, device=%s\n",
			inet_ntoa( pnode->sockAddr.sin_addr ),
			pnode->record[SIGNAL_MAGNET_ID].val.si );
	}
	else
	{
	    if ( pnode->record[SIGNAL_MAGNET_ID].val.si[0] )
	    {
		printf( "  EtherPSC %15s disconnected, device=%s\n",
			inet_ntoa( pnode->sockAddr.sin_addr ),
			pnode->record[SIGNAL_MAGNET_ID].val.si );
	    }
	    else
	    {
		printf( "  EtherPSC %15s disconnected\n",
			inet_ntoa( pnode->sockAddr.sin_addr ) );
	    }
	}
    }

    return(OK);
}

/* Driver report routines */

static long report( int level )
{
    return( Ether_io_report(level) );
}


long etherPSCdrvInitRecord( struct bitbusio *pb, struct dbCommon *pr )
{
    unsigned long	addr;
    ETHERPSCNODE	*node, **pnode;

    if ( pb->signal < 1  ||  pb->signal > SIGNAL_MAXNUM )
    {
        printf( "etherPSCdrvInitRecord: invalid signal #%d encountered!\n",
                        pb->signal );
        return (ERROR);
    }

    if ( pb->link != 0 )
    {
        printf( "etherPSCdrvInitRecord: invalid link #%d encountered!\n",
                        pb->link );
        return (ERROR);
    }

    if ( ( addr = inet_addr(  pb->parm ) ) == ERROR )
    {
        printf( "etherPSCdrvInitRecord: invalid ip address %s encountered!\n",
                        pb->parm );
        return (ERROR);
    }

    for ( pnode = (ETHERPSCNODE**) &etherpsc.pnode;
		(node = *pnode) && node->sockAddr.sin_addr.s_addr != addr;
		pnode = (ETHERPSCNODE**) &node->pnode ) {};
    if ( ! node )
    {
	node = malloc( sizeof(ETHERPSCNODE) );
	*pnode = node;

	if ( ! node )
	{
	    printf( "etherPSCdrvInitRecord: malloc failed\n" );
	    return( ERROR );
	}
	memset( node, 0, sizeof(ETHERPSCNODE) );
	node->sockAddrSize = sizeof(node->sockAddr);
	node->sockAddr.sin_addr.s_addr = addr;
	node->sockAddr.sin_family = AF_INET;
	node->present = 0;
    }

    node->record[pb->signal].precord = pr;
    pr->dpvt = &node->record[pb->signal];

    return (2);
}



static void node_gone ( ETHERPSCNODE *node )
{
    int                 i;
    struct rset         *prset;
    struct dbCommon     *record;

    if ( node->present ) {

	printf( "drvEtherPSC: lost node %s\n",
			inet_ntoa( node->sockAddr.sin_addr ) );

				/* PSC gone, cancel all queued commands */
	node->record[SIGNAL_PS_ON_OFF].set = 0;
	node->record[SIGNAL_INT_RESET].set = 0;
	node->record[SIGNAL_CURRENT_AC].set = 0;

        for ( i=9; i<SIGNAL_MAXNUM+1; i++ )
        {
                                /* PSC gone, UDF all output records */
            if ( (record = node->record[i].precord)  &&
                      ! node->record[i].nsta ) {
                node->record[i].nsta = COMM_ALARM;
                prset = (struct rset *) record->rset;
                dbScanLock(record);
                record->pact = TRUE;
                record->udf  = TRUE;
                (*prset->process)(record);
                dbScanUnlock(record);
            }
        }
        node->present = 0;
    }
}


/*
 * send_msg
 * send message to ethernet power supply controler
 */
static long send_msg( ETHERPSCNODE *node, const ETHERPSCMSG *msg )
{

    node->sockAddr.sin_port = htons( msg->port );

    if ( node->unanswered > 10 ) {
	node_gone( node );
    }
    else node->unanswered++;

    if ( sendto( etherpsc.sock, &msg->cmd, msg->datalength, 0,
	(struct sockaddr*) &node->sockAddr, node->sockAddrSize ) == ERROR ) {
	if ( node->present ) {
	    perror( "drvEtherPSC: sendto" );
	    node_gone( node );
	}
	return( 1 );
    }

					/* count message */
    node->record[SIGNAL_BITBUS_CMD_CNT].val.li++;

    return( OK );
}

/*
 * d2b
 * Convert double into Bitbus PSC byte stream
 */
static void d2b ( unsigned char *b, double *d )
{
    float               f;
    unsigned char       *p;

    p = (unsigned char*) &f;
    f = *d;
#ifdef __PPC__    
    b[0] = p[3];
    b[1] = p[2];
    b[2] = p[1];
    b[3] = p[0];
#else
    b[0] = p[0];
    b[1] = p[1];
    b[2] = p[2];
    b[3] = p[3];
#endif

    return;
}

/*
 * s2b
 * Convert short into Bitbus PSC byte stream
 */
static void s2b ( unsigned char *b, unsigned short s )
{
    b[0] = s & 0xff;
    b[1] = s >> 8;

    return;
}

/*
 * b2f
 * Convert Bitbus PSC byte stream into float
 */
static float b2f ( unsigned char *b )
{
    float               f;
    unsigned char       *p;

    p = (unsigned char*) &f;
#ifdef __PPC__    
    p[0] = b[3];
    p[1] = b[2];
    p[2] = b[1];
    p[3] = b[0];
#else
    p[0] = b[0];
    p[1] = b[1];
    p[2] = b[2];
    p[3] = b[3];
#endif

    return ( f );
}

/*
 * b2l
 * Convert Bitbus PSC byte stream (2 bytes) into long
 */
static long b2l ( unsigned char *b )
{
    long                l;
    unsigned short      us;
    short               *s;

    us = ( b[1] << 8 ) + b[0];
    s = (short*) &us;
    l = *s;

    return ( l );
}



static EPICSTHREADFUNC etherPSC_output_thread( ETHERPSC *etherpsc )
{
    int			phase;
    ETHERPSCNODE	*node;
    ETHERPSCMSG		msg;
    double		new_ao, dI, dIdt;
    int			dt100;
    char		*p;

    printf( "drvEtherPSC: transmitter thread launched\n" );

    for ( node = etherpsc->pnode; node;
                node = (ETHERPSCNODE*) node->pnode )
    {
	p = inet_ntoa( node->sockAddr.sin_addr );
        process_record_si( node, SIGNAL_CNTL_ADDRESS, (unsigned char *)p, strlen(p) );
        process_record_li( node, SIGNAL_BITBUS_LINE, 0 );
        process_record_li( node, SIGNAL_BITBUS_ADDRESS, 0 );


                                        /* request old current setpoint */
        send_msg( node, &DES_I_RDBK_MSG );
    }


                                        /* it takes three phases to get */
                                        /* all information from PSC */
    for ( phase = 0; ; phase = (phase + 1) % 3 )
    {
        epicsThreadSleep( EtherPSCDelay );      /* delay between cycles */

        for ( node = etherpsc->pnode; node;
                node = (ETHERPSCNODE*) node->pnode )
        {
	    if ( node->rampwait > 0 ) node->rampwait--;  /* Wait for ramp to start */
            if ( node->busy > 0 )
            {                           /* PSC needs more time, skip it */
                node->busy--;
                continue;
            }

            if ( node->record[SIGNAL_PS_ON_OFF].set )
            {                           /* turn PSC on or off */
		node->record[SIGNAL_PS_ON_OFF].set = 2;
                if ( node->record[SIGNAL_PS_ON_OFF].val.bo )
                {
                    send_msg( node, &PS_ON_MSG );
                    node->busy = 20;     /* this will take a while */
                    if (!node->record[SIGNAL_ON_STATUS].val.bi)
                      process_record_ao( node, SIGNAL_CURRENT_AC, (float) 0.0 );
                }
                else
                {
                    send_msg( node, &PS_OFF_MSG );
                    node->busy = 10;     /* give it some time */
                }
                continue;
            }

            if ( node->record[SIGNAL_INT_RESET].set )
            {                           /* execute RESET command */
                send_msg( node, &PS_INT_RESET_MSG );
                node->busy = 10;         /* give it some time */
                continue;
            }

            if ( node->record[SIGNAL_INFO_MSG].set )
            {
                                        /* there is a message to grab */
                node->record[SIGNAL_INFO_MSG].set = 0;
                send_msg( node, &INFO_MSG_MSG );
                continue;
            }

            if ( node->record[SIGNAL_CURRENT_AC].set  &&
		 node->rampwait <= 0                  &&
                 node->record[SIGNAL_RAMPING_STATUS].val.bi == 0 )
            {                           /* we have a setpoint to send */
                        /* don't send setpoint, if PS is off */
                if ( node->record[SIGNAL_ON_STATUS].val.bi == 0 ) {
		    node->record[SIGNAL_CURRENT_AC].set = 0;	/* done */
		    continue;
		}
		node->record[SIGNAL_CURRENT_AC].set = 2;
		node->rampwait = 2;

                new_ao = node->record[SIGNAL_CURRENT_AC].val.ao.next;

                                        /* calculate ramping time */
                dI = new_ao - node->record[SIGNAL_CURRENT_AC].val.ao.last;
                if ( dI < 0.0 ) dI = - dI;
                dIdt = node->record[SIGNAL_CURRENT_AC_DIDT].val.ao.next;
                if ( dIdt  > 0.0 )
                {
                    dt100 = dI / dIdt * 100.0 + 1.0;
                    dt100 = dt100 * 3.154/2.0;  /* correct for cosine curve */
                    if ( dt100 > 0xffff ) dt100 = 0xffff;
                }
                else
                {
                    dt100 = 1;
                }
                node->record[SIGNAL_CURRENT_AC].val.ao.last = new_ao;
                msg = PS_CURRENT_AC_MSG;
                d2b( &msg.data[3], &new_ao );
                s2b( &msg.data[7], (unsigned short) dt100 );
                send_msg( node, &msg );
                continue;
            }

            if ( node->record[SIGNAL_CHASSIS_TYPE].set )
            {                           /* controller available, */
                                        /* get static configuration info */
                send_msg( node, &CNTL_INFO_MSG );
                continue;
            }

            if ( node->record[SIGNAL_XDUCT1_V2I].set )
            {
                send_msg( node, &CAL_DATA_MSG );
                continue;
            }

            switch ( phase )
            {
                case 0 :
                    send_msg( node, &FAST_STATUS_MSG );
                break;

                case 1 :
                    send_msg( node, &DIAG_STATUS_MSG );
                break;

                case 2 :
                    send_msg( node, &ANALOG_RDBK_MSG );
                break;
            }
        }
    }

    return(OK);
}


static EPICSTHREADFUNC etherPSC_input_thread( ETHERPSC *etherpsc )
{
    int			n;
    unsigned char	rsp[512];
    struct sockaddr_in	sockAddr;
    socklen_t	        sockAddrSize;
    ETHERPSCNODE	*node;

    printf( "drvEtherPSC: receiver thread launched\n" );

    sockAddrSize = sizeof(sockAddr);

    for ( ; ; )
    {
	if ( ( n = recvfrom( etherpsc->sock, rsp, sizeof(rsp), 0,
			(struct sockaddr*) &sockAddr, &sockAddrSize ) ) < 0 ) 
	{
	    perror( "drvEtherPSC: recvfrom" );
	    return ( 0 );		/* needs further checking */
	}

#if DEBUG > 0
	printf("%d bytes from %s:%d\n", n, inet_ntoa(sockAddr.sin_addr),
			ntohs(sockAddr.sin_port) );
#endif

	if ( n > 2 )
	{
					/* find node description */
	    for ( node = etherpsc->pnode;
		node  && 
		node->sockAddr.sin_addr.s_addr != sockAddr.sin_addr.s_addr;
		node = (ETHERPSCNODE*) node->pnode ) { };
	    if ( node ) 
	    {
					/* count response */
		node->record[SIGNAL_BITBUS_RSP_CNT].val.li++;

		node->unanswered = 0;
	
		if ( ! node->present )
		{			/* PSC became available */
					/* trigger reading static info */
		    node->record[SIGNAL_CHASSIS_TYPE].set = 1;
		    node->record[SIGNAL_XDUCT1_V2I].set = 1;
		    node->present = 1;
		}

					/* process PSC response */
					/* skip command byte as we now have */
					/* the returned command byte */
		process_etherpsc_rsp( node, &rsp[2], n-2 );
	
	    }
	}
    }
    return(OK);
}


static void process_record_ai( ETHERPSCNODE *node, int signal, float f )
{
    PSCRECORD           *PSCRecord;
    aiRecord            *pai;
    struct rset         *prset;

    PSCRecord = &node->record[signal];
    if ( ! (pai = (aiRecord*) PSCRecord->precord) ) return;

    if ( pai->val != f  ||  pai->udf  ||  PSCRecord->nsta )
    {
        PSCRecord->nsta = NO_ALARM;
        PSCRecord->val.ai = f;
        prset = (struct rset *) pai->rset;

        dbScanLock( (struct dbCommon*) pai );
        pai->pact = TRUE;
        (*prset->process)( pai );
        dbScanUnlock( (struct dbCommon*) pai );
    }
}

static void process_record_ao( ETHERPSCNODE *node, int signal, float f )
{
    PSCRECORD           *PSCRecord;
    aoRecord            *pao;
    struct rset         *prset;

    PSCRecord = &node->record[signal];
    if ( ! (pao = (aoRecord*) PSCRecord->precord) ) return;

    prset = (struct rset *) pao->rset;

    dbScanLock( (struct dbCommon*) pao );
    pao->val = f;
    (*prset->process)( pao );
    dbScanUnlock( (struct dbCommon*) pao );
}

static void process_record_bi( ETHERPSCNODE *node, int signal, int i )
{
    PSCRECORD           *PSCRecord;
    biRecord            *pbi;
    struct rset         *prset;

    PSCRecord = &node->record[signal];
    if ( ! (pbi = (biRecord*) PSCRecord->precord) ) return;

    if ( pbi->rval != i  ||  pbi->udf  ||  PSCRecord->nsta )
    {
        PSCRecord->nsta = NO_ALARM;
        PSCRecord->val.bi = i;
        prset = (struct rset *) pbi->rset;

        dbScanLock( (struct dbCommon*) pbi );
        pbi->pact = TRUE;
        (*prset->process)( pbi );
        dbScanUnlock( (struct dbCommon*) pbi );
    }
}

static void process_record_li( ETHERPSCNODE *node, int signal, long i )
{
    PSCRECORD           *PSCRecord;
    struct longinRecord *pli;
    struct rset         *prset;

    PSCRecord = &node->record[signal];
    if ( ! (pli = (struct longinRecord*) PSCRecord->precord) ) return;

    if ( pli->val != i  ||  pli->udf  ||  PSCRecord->nsta )
    {
        PSCRecord->nsta = NO_ALARM;
        PSCRecord->val.li = i;
        prset = (struct rset *) pli->rset;

        dbScanLock( (struct dbCommon*) pli );
        pli->pact = TRUE;
        (*prset->process)( pli );
        dbScanUnlock( (struct dbCommon*) pli );
    }
}

static void process_record_si( ETHERPSCNODE *node, int signal, unsigned char *s,
                                                int l )
{
    PSCRECORD           *PSCRecord;
    struct stringinRecord       *psi;
    struct rset         *prset;

    PSCRecord = &node->record[signal];
    if ( ! (psi = (struct stringinRecord*) PSCRecord->precord) ) return;

    PSCRecord->nsta = NO_ALARM;
    if ( l > sizeof(PSCRecord->val.si) ) l = sizeof(PSCRecord->val.si);
    strncpy( PSCRecord->val.si, (const char*) s, l );
    if ( l < sizeof(PSCRecord->val.si) ) PSCRecord->val.si[l] = '\0';
    prset = (struct rset *) psi->rset;

    dbScanLock( (struct dbCommon*) psi );
    psi->pact = TRUE;
    (*prset->process)( psi );
    dbScanUnlock( (struct dbCommon*) psi );
}

static void process_status1 ( ETHERPSCNODE *node, unsigned char *rsp )
{
    int         i;

    i = ( rsp[2] & 0x04 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_ON_STATUS, i );
    if ( i == 0 ) node->record[SIGNAL_CURRENT_AC].val.ao.last = 0.0;

    i = ( rsp[2] & 0x08 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_RAMPING_STATUS, i );
    if (i) node->rampwait = 0; 

    i = ( rsp[2] & 0x80 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_LOCAL_MODE, i );

    i = ( rsp[3] & 0x02 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_HW_STATUS, i );

    i = ( rsp[3] & 0x04 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_CAL_STATUS, i );

    i = ( rsp[3] & 0x08 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_XDUCT2_READY, i );

    if ( rsp[0] == BITBUSCMD_FAST_STATUS  &&
         rsp[3] & 0x01 )                        /* error message available */
    {
        node->record[SIGNAL_INFO_MSG].set = 1;
    }

}

static void process_status3 ( ETHERPSCNODE *node, unsigned char *rsp )
{
    int         i;

    i = ( rsp[4] & 0x01 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_INTERLOCK0, i );

    i = ( rsp[4] & 0x02 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_INTERLOCK1, i );

    i = ( rsp[4] & 0x04 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_INTERLOCK2, i );

    i = ( rsp[4] & 0x08 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_INTERLOCK3, i );

    i = ( rsp[4] & 0x10 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_READY_STATUS, i );

    i = ( rsp[4] & 0x20 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_XDUCT1_STATUS, i );

    i = ( rsp[4] & 0x40 ) ? 0 : 1;
    process_record_bi( node, SIGNAL_GND_CURRENT_FAULT, i);

    i = ( rsp[5] & 0x01 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_FAULT_LATCH_STATUS, i );

    i = ( rsp[5] & 0x02 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_PS_STATUS0, i );

    i = ( rsp[5] & 0x04 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_PS_STATUS1, i );

    i = ( rsp[5] & 0x08 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_PS_STATUS2, i );

    i = ( rsp[5] & 0x10 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_PS_STATUS3, i );

    i = ( rsp[5] & 0x20 ) ? 1 : 0;
    process_record_bi( node, SIGNAL_PS_ON_STATUS, i );
}

static void process_etherpsc_rsp ( ETHERPSCNODE *node, unsigned char *rsp, long n )
{
    int         i;

    node->busy = 0;

    switch ( rsp[0] )
    {
        case BITBUSRSP_PS_STATUS :
        case BITBUSCMD_SHORT_STATUS :
        case BITBUSCMD_FAST_STATUS :

            if ( n < 4  ||  n > 8 ) return;
            if ( ! ( rsp[2] & 0x01 ) ) return;
#if  DEBUG
            printf( "STATUS_RDBK: 0x%02x, 0x%02x, %.3f\n",
                        rsp[2], rsp[3], b2f(&rsp[4]) );
#endif

            process_status1 ( node, rsp );

            if ( n < 8 ) return;

            process_record_ai( node, SIGNAL_CURRENT, b2f(&rsp[4]) );

        break;

        case BITBUSCMD_ANALOG_RDBK :

            if ( n < 34 ) return;
#if  DEBUG
            printf(
        "ANALOG_RDBK: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n",
                        b2f(&rsp[2]), b2f(&rsp[6]), b2f(&rsp[10]),
                        b2f(&rsp[14]), b2f(&rsp[18]), b2f(&rsp[22]),
                        b2f(&rsp[26]), b2f(&rsp[30]) );

#endif

            process_record_ai( node, SIGNAL_XDUCT1_CURRENT, b2f(&rsp[2]) );
            process_record_ai( node, SIGNAL_XDUCT2_CURRENT, b2f(&rsp[6]) );
            process_record_ai( node, SIGNAL_DAC_CURRENT, b2f(&rsp[10]) );
            process_record_ai( node, SIGNAL_RIPPLE_CURRENT, b2f(&rsp[14]) );
            process_record_ai( node, SIGNAL_GND_CURRENT, b2f(&rsp[18]) );
            process_record_ai( node, SIGNAL_CNTL_TEMPERATURE,  b2f(&rsp[22]) );
            process_record_ai( node, SIGNAL_OUTPUT_VOLTAGE, b2f(&rsp[26]) );
            process_record_ai( node, SIGNAL_SPARE_VOLTAGE, b2f(&rsp[30]) );

        break;

        case BITBUSCMD_DIAG_RDBK :

            if ( n < 31 ) return;
#if  DEBUG
            printf(
        "DIAG_RDBK: 0x%02x, 0x%02x, 0x%02x, 0x%02x, %d\n",
                        rsp[2], rsp[3], rsp[4], rsp[5], rsp[6] );
                printf(
        "           %.3f, %.3f, %.3f\n",
                        b2f(&rsp[7]), b2f(&rsp[11]), b2f(&rsp[15]) );
                printf(
        "           %05ld, %05ld, %05ld, %05ld, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                        b2l(&rsp[19]), b2l(&rsp[21]), b2l(&rsp[23]),
                        b2l(&rsp[25]),
                        rsp[27], rsp[28], rsp[29], rsp[30] );
#endif

            process_status1 ( node, rsp );
            process_status3 ( node, rsp );
            process_record_ai( node, SIGNAL_CURRENT_READBACK, b2f(&rsp[7]) );
            process_record_li( node, SIGNAL_ADC_OFFSET, b2l(&rsp[19]) );
            process_record_li( node, SIGNAL_ADC_GAIN, b2l(&rsp[21]) );
            process_record_li( node, SIGNAL_DAC_OFFSET, b2l(&rsp[23]) );
            process_record_li( node, SIGNAL_DAC_GAIN, b2l(&rsp[25]) );
            process_record_li( node, SIGNAL_LAST_RESET_CODE, rsp[27] );
            process_record_li( node, SIGNAL_LAST_TURNOFF_CODE, rsp[28] );
            process_record_li( node, SIGNAL_CAL_ERROR_CODE, rsp[29] );
            process_record_li( node, SIGNAL_SELF_TEST_CODE, rsp[30] );

        break;

        case BITBUSCMD_CNTL_INFO :

            if ( n < 26 ) return;
#if  DEBUG
            printf( "CNTL_INFO: %d, %.8s, %.8s, %.8s\n",
                        rsp[2], &rsp[3], &rsp[11], &rsp[19] );
#endif
            node->record[SIGNAL_CHASSIS_TYPE].set = 0;	/* ack, done */

            process_record_li( node, SIGNAL_CHASSIS_TYPE, (int) rsp[2] );
            process_record_si( node, SIGNAL_SERIAL_NUMBER, &rsp[3], 8 );
            process_record_si( node, SIGNAL_FIRMWARE_VERSION, &rsp[11], 8 );
            process_record_si( node, SIGNAL_MAGNET_ID, &rsp[19], 8 );

        break;

        case BITBUSCMD_CAL_DATA :

            if ( n < 29 ) return;

#if  DEBUG
            printf( "CAL_DATA: %.3f, %.3f, %.3f, %.3f, %.3f, %.8s\n",
                        b2f(&rsp[2]), b2f(&rsp[6]), b2f(&rsp[10]),
                        b2f(&rsp[14]), b2f(&rsp[18]), &rsp[22] );
#endif
            node->record[SIGNAL_XDUCT1_V2I].set = 0;	/* ack, done */

            process_record_ai( node, SIGNAL_XDUCT1_V2I, b2f(&rsp[2]) );
            process_record_ai( node, SIGNAL_XDUCT2_V2I, b2f(&rsp[6]) );
            process_record_ai( node, SIGNAL_GND_CURRENT_V2I, b2f(&rsp[10]) );
            process_record_ai( node, SIGNAL_PS_VOLTAGE_V2V, b2f(&rsp[14]) );
            process_record_ai( node, SIGNAL_VREF, b2f(&rsp[18]) );
            process_record_si( node, SIGNAL_CAL_DATE, &rsp[22], 8 );

        break;

        case BITBUSCMD_DES_I_RDBK :

            if ( n < 9 ) return;

#if  DEBUG
            printf( "DES_I_RDBK: 0x%02x, 0x%02x, %.3f, %ld\n",
                        rsp[2], rsp[3], b2f(&rsp[4]), b2l(&rsp[8]) );
#endif
            process_status1( node, rsp );
            node->record[SIGNAL_CURRENT_AC].val.ao.last = b2f(&rsp[4]);
            process_record_ao( node, SIGNAL_CURRENT_AC, b2f(&rsp[4]) );

        break;

        case BITBUSCMD_INFO_MSG :

            if ( n < 3 ) return;
            if ( n > 34 ) return;

#if  DEBUG
            printf( "INFO_MSG: %.32s\n", &rsp[2] );
#endif

            process_record_si( node, SIGNAL_INFO_MSG, &rsp[2], n-2 );

        break;

	case BITBUSCMD_SET_I :

			/* if desired value has not been changed in between */
	    if ( node->record[SIGNAL_CURRENT_AC].set == 2 )
        	node->record[SIGNAL_CURRENT_AC].set = 0;    /* ack, done */
				/* otherwise output thread will try again */

            process_status1( node, rsp );

	break;

	case BITBUSCMD_PS_ON :
	case BITBUSCMD_PS_OFF :

	    if ( n < 4 ) return;

                        /* if desired state has not been changed in between */
            if ( node->record[SIGNAL_PS_ON_OFF].set == 2 )
                node->record[SIGNAL_PS_ON_OFF].set = 0;    /* ack, done */
                                /* otherwise output thread will try again */

	    process_status1( node, rsp );
 
	break;
 
	case BITBUSCMD_INT_RESET :

            node->record[SIGNAL_INT_RESET].set = 0;    /* ack, done */
 
            process_status1( node, rsp );

	break;

        default :
                printf("rsp:");
                for ( i=0; i<n; i++ ) {
                    printf(" %02x", rsp[i] );
                }
                printf("\n");
        break;
    }
}


/*******************************************************************************
* EPICS iocsh Command registry
*/

/* Ether_io_report ( int level ) */
static const iocshArg Ether_io_reportArg0 = {"level", iocshArgInt};
static const iocshArg * const Ether_io_reportArgs[1] = {&Ether_io_reportArg0};
static const iocshFuncDef Ether_io_reportFuncDef =
    {"Ether_io_report",1,Ether_io_reportArgs};
static void Ether_io_reportCallFunc(const iocshArgBuf *args)
{
    Ether_io_report(args[0].ival);
}

/* drvEtherPSCConfig( unsigned long *base_addr, short irq, short link ) */
static const iocshArg drvEtherPSCConfigArg0 = {"ip_addr", iocshArgString};
static const iocshArg drvEtherPSCConfigArg1 = {"link", iocshArgInt};
static const iocshArg * const drvEtherPSCConfigArgs[2] = {
    &drvEtherPSCConfigArg0, &drvEtherPSCConfigArg1};
static const iocshFuncDef drvEtherPSCConfigFuncDef =
    {"drvEtherPSCConfig",2,drvEtherPSCConfigArgs};
/* static void drvEtherPSCConfigCallFunc(const iocshArgBuf *arg)
{
    drvEtherPSCConfig(arg[0].sval, arg[1].ival);
} */

LOCAL void drvEtherPSCRegistrar(void) {
    iocshRegister(&Ether_io_reportFuncDef,Ether_io_reportCallFunc);
/*    iocshRegister(&drvEtherPSCConfigFuncDef,drvEtherPSCConfigCallFunc); */
}
epicsExportRegistrar(drvEtherPSCRegistrar);
