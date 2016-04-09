/* devEtherPSC.c */
/* SPEAR Ethernet Power Supply Controller device support module */
  
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "errlog.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "aiRecord.h"
#include "aoRecord.h"
#include "biRecord.h"
#include "boRecord.h"
#include "longinRecord.h"
#include "stringinRecord.h"
#include "link.h"
#include "epicsExport.h"

#include "etherPSCInclude.h"

#define	DEBUG		0


/*Create the dsets */

static long init_airecord();
static long read_ai();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiEtherPSC={
	6,
	NULL,
	NULL,
	init_airecord,
	NULL,
	read_ai,
	NULL,
};
epicsExportAddress(dset, devAiEtherPSC);

static long init_aorecord();
static long write_ao();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoEtherPSC={
	6,
	NULL,
	NULL,
	init_aorecord,
	NULL,
	write_ao,
	NULL,
};
epicsExportAddress(dset, devAoEtherPSC);

static long init_bi_record();
static long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiEtherPSC={
	5,
	NULL,
	NULL,
	init_bi_record,
	NULL,
	read_bi,
};
epicsExportAddress(dset, devBiEtherPSC);

static long init_bo_record();
static long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoEtherPSC={
	5,
	NULL,
	NULL,
	init_bo_record,
	NULL,
	write_bo,
};
epicsExportAddress(dset, devBoEtherPSC);

static long init_li_record();
static long read_li();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin;
}devLiEtherPSC={
	5,
	NULL,
	NULL,
	init_li_record,
	NULL,
	read_li,
};
epicsExportAddress(dset, devLiEtherPSC);

static long init_si_record();
static long read_si();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
}devSiEtherPSC={
	5,
	NULL,
	NULL,
	init_si_record,
	NULL,
	read_si,
};
epicsExportAddress(dset, devSiEtherPSC);

static long init_airecord( struct aiRecord *pai )
{

    struct bitbusio 	*pb = (struct bitbusio*)&(pai->inp.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) pai ) );
}

static long read_ai( struct aiRecord *pai)
{
    PSCRECORD		*PSCRecord;

#if  DEBUG
    struct bitbusio     *pb = (struct bitbusio*)&(pai->inp.value);

    epicsPrintf("read_ai@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
		pb->link, pb->node, pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = pai->dpvt) ) return (2);

    if ( PSCRecord->nsta ) recGblSetSevr( pai, PSCRecord->nsta, INVALID_ALARM );

    pai->val  = PSCRecord->val.ai;
    pai->udf  = FALSE;
    pai->pact = FALSE;

    return(2);
}


static long init_aorecord( struct aoRecord *pao )
{

    struct bitbusio	*pb = (struct bitbusio*)&(pao->out.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) pao ) );

}

static long write_ao( struct aoRecord *pao )
{
    PSCRECORD		*PSCRecord;

#if  DEBUG
    struct bitbusio     *pb = (struct bitbusio*)&(pao->out.value);

    epicsPrintf("write_ao@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
		pb->link, pb->node, pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = pao->dpvt) ) return (2);

    PSCRecord->val.ao.next = pao->val;
    PSCRecord->set = 1;

    return(0);
}


static long init_bi_record( struct biRecord *pbi )
{

    struct bitbusio 	*pb = (struct bitbusio*)&(pbi->inp.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) pbi ) );

}

static long read_bi( struct biRecord *pbi )
{
    PSCRECORD		*PSCRecord;

#if  DEBUG
    struct bitbusio     *pb = (struct bitbusio*)&(pbi->inp.value);

    epicsPrintf("read_bi@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
		pb->link, pb->node, pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = pbi->dpvt) ) return (2);

    if ( PSCRecord->nsta ) recGblSetSevr( pbi, PSCRecord->nsta, INVALID_ALARM );

    pbi->rval = PSCRecord->val.bi;
    pbi->udf  = FALSE;
    pbi->pact = FALSE;

    return(0);
}


static long init_bo_record( struct boRecord *pbo )
{

    struct bitbusio     *pb = (struct bitbusio*)&(pbo->out.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) pbo ) ); 

}

static long write_bo( struct boRecord *pbo )
{
    PSCRECORD		*PSCRecord;
    struct bitbusio     *pb = (struct bitbusio*)&(pbo->out.value);


#if  DEBUG
    epicsPrintf("write_bo@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
		pb->link, pb->node, pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = pbo->dpvt) ) return (2);

    switch ( pb->signal )
    {
	case	SIGNAL_PS_ON_OFF :
		PSCRecord->val.bo = pbo->val;
		PSCRecord->set = 1;	
	break;

	case	SIGNAL_INT_RESET :
		if ( pbo->val ) PSCRecord->set = 1;	
	break;
    }

    return(0);
}
 

static long init_li_record( struct longinRecord *pli )
{

    struct bitbusio 	*pb = (struct bitbusio*)&(pli->inp.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) pli ) );

}

static long read_li( struct longinRecord *pli )
{
    PSCRECORD		*PSCRecord;

#if  DEBUG
    struct bitbusio     *pb = (struct bitbusio*)&(pli->inp.value);

    epicsPrintf ( "read_li@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
		pb->link, pb->node,pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = pli->dpvt) ) return (2);

    if ( PSCRecord->nsta ) recGblSetSevr( pli, PSCRecord->nsta, INVALID_ALARM );

    pli->val  = PSCRecord->val.li;
    pli->udf  = FALSE;
    pli->pact = FALSE;

    return(0);
}


static long init_si_record( struct stringinRecord *psi )
{

    struct bitbusio 	*pb = (struct bitbusio*)&(psi->inp.value);

    return ( etherPSCdrvInitRecord( pb, (struct dbCommon*) psi ) );
}

static long read_si( struct stringinRecord *psi )
{

    PSCRECORD		*PSCRecord;

#if  DEBUG
    struct bitbusio     *pb = (struct bitbusio*)&(psi->inp.value);

    epicsPrintf ( "read_si@devEtherPSC, L=%d, N=%d, P=%d, S=%d\n",
                pb->link, pb->node,pb->port, pb->signal );
#endif

    if ( ! (PSCRecord = psi->dpvt) ) return (2);

    if ( PSCRecord->nsta ) recGblSetSevr( psi, PSCRecord->nsta, INVALID_ALARM );

    strncpy( psi->val, PSCRecord->val.si, sizeof(psi->val) );
    psi->oval[0]=0;
    psi->udf  = FALSE;
    psi->pact = FALSE;

    return ( 0 );
}

