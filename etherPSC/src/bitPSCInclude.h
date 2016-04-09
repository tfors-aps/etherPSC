#define SIGNAL_PS_ON_OFF        1
#define SIGNAL_INT_RESET        2
#define SIGNAL_CURRENT_AC       3
#define	SIGNAL_CURRENT_AC_DIDT	4
#define	SIGNAL_RAMPING_STATUS	9
#define SIGNAL_ON_STATUS        10
#define SIGNAL_READY_STATUS     11
#define SIGNAL_INTERLOCK0       12
#define SIGNAL_INTERLOCK1       13
#define SIGNAL_INTERLOCK2       14
#define SIGNAL_INTERLOCK3       15
#define SIGNAL_XDUCT1_STATUS    16
#define SIGNAL_GND_CURRENT_FAULT 17
#define SIGNAL_FAULT_LATCH_STATUS 18
#define SIGNAL_PS_ON_STATUS     19
#define SIGNAL_PS_STATUS0       20
#define SIGNAL_PS_STATUS1       21
#define SIGNAL_PS_STATUS2       22
#define SIGNAL_PS_STATUS3       23
#define SIGNAL_XDUCT2_READY     24
#define SIGNAL_LOCAL_MODE       25
#define SIGNAL_CAL_STATUS       26
#define SIGNAL_HW_STATUS        27
#define SIGNAL_BITBUS_ADDRESS   28
#define SIGNAL_CHASSIS_TYPE     29
#define SIGNAL_SERIAL_NUMBER    30
#define	SIGNAL_FIRMWARE_VERSION 31
#define SIGNAL_MAGNET_ID        32
#define SIGNAL_CURRENT          33
#define SIGNAL_CURRENT_READBACK 34
#define SIGNAL_XDUCT1_CURRENT   35
#define SIGNAL_XDUCT2_CURRENT   36
#define SIGNAL_DAC_CURRENT      37
#define SIGNAL_RIPPLE_CURRENT   38
#define SIGNAL_GND_CURRENT      39
#define SIGNAL_OUTPUT_VOLTAGE   40
#define SIGNAL_CNTL_TEMPERATURE 41
#define SIGNAL_SPARE_VOLTAGE    42
#define SIGNAL_XDUCT1_V2I       43
#define SIGNAL_XDUCT2_V2I       44
#define SIGNAL_GND_CURRENT_V2I  45
#define SIGNAL_PS_VOLTAGE_V2V   46
#define SIGNAL_VREF             47
#define SIGNAL_CAL_DATE         48
#define SIGNAL_ADC_OFFSET       49
#define SIGNAL_ADC_GAIN         50
#define SIGNAL_DAC_OFFSET       51
#define SIGNAL_DAC_GAIN         52
#define SIGNAL_LAST_RESET_CODE  53
#define SIGNAL_SELF_TEST_CODE   54
#define SIGNAL_LAST_TURNOFF_CODE 55
#define SIGNAL_CAL_ERROR_CODE   56
#define	SIGNAL_INFO_MSG		57
#define SIGNAL_BITBUS_LINE	58
#define SIGNAL_BITBUS_CMD_CNT	59
#define	SIGNAL_BITBUS_RSP_CNT	60
#define SIGNAL_CNTL_ADDRESS	61

#define SIGNAL_MAXNUM           61

#define	BITBUS_MAX_CARDS	21
#define	BITBUS_MAX_ADDRESS	254

#define BITBUSCMD_SHORT_STATUS  0xc0
#define BITBUSCMD_SET_I         0xc1
#define BITBUSCMD_SETUPRAMP     0xc2
#define BITBUSCMD_DES_I_RDBK    0xc3
#define BITBUSCMD_INT_RESET     0xc4
#define BITBUSCMD_PS_OFF        0xc5
#define BITBUSCMD_PS_ON         0xc6
#define BITBUSCMD_PS_ON_REV     0xc7
#define BITBUSCMD_ANALOG_RDBK   0xc8
#define BITBUSCMD_INFO_MSG      0xc9
#define BITBUSCMD_DIAG_RDBK     0xca
#define BITBUSCMD_CNTL_INFO     0xcb
#define BITBUSCMD_CAL_DATA      0xcc
#define BITBUSCMD_FAST_STATUS   0xcd
#define BITBUSCMD_CNTL_RESET    0xe3

#define BITBUSRSP_PS_STATUS     0xbf


typedef struct
        {
            volatile unsigned char      x1, csr;
            volatile unsigned char      x2, iack;
            volatile unsigned char      x3, data;
            volatile unsigned char      x4, cmd;
            volatile unsigned char      x5, fifocsr1;
            volatile unsigned char      x6, irq;
            volatile unsigned char      x7, fifocsr2;
        } VEBBMAP;

typedef struct
        {
            struct dbCommon     *precord;
	    int			nsta;
	    int			set;
	    union
	    {
		float	ai;
		struct	
		{
		    double	next;
		    double	last;
		} ao;
		int	bi;
		int	bo;
		long	li;
		char	si[40];
	    } val;
        } PSCRECORD;

typedef struct
	{
	    struct PSCNODE	*pnode;
	    unsigned short	address;
	    unsigned short	link;
	    unsigned short	present;
	    unsigned short	busy;
	    PSCRECORD		record[SIGNAL_MAXNUM+1];
	} PSCNODE;

typedef struct
	{
	    unsigned short	defined;
	    unsigned short	present;
	    unsigned short	irq;
	    void		*base_addr;
	    VEBBMAP		*paddr;
	    PSCNODE		*pnode;
	} BITBUS;

long	drvInitRecord( struct bitbusio*, struct dbCommon* );

