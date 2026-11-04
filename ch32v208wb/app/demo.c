/* Standalone USBFS demo core. Hardware adaptation follows the WCH EVT USBFS
 * controller model; preserve WCH SDK notices in the accompanying vendor tree. */
#include "demo.h"
#include <string.h>

uint8_t demo_ep0[64] __attribute__((aligned(4)));
volatile uint8_t demo_brightness;
volatile uint8_t demo_configured;
volatile uint32_t demo_usb_irqs;
static uint8_t scratch[64];
static const uint8_t *reply;
static unsigned residue;
static uint8_t ep0_toggle, control_in, send_zlp, address_pending, pending_address;
static uint32_t ep2_completions;
static uint16_t debounce_ms = 5;

static const uint8_t device_descriptor[] = {
    18,1,0x10,2,0,0,0,64,0x09,0x12,1,0,8,2,1,2,3,1
};
static const uint8_t config_descriptor[] = {
    9,2,32,0,1,1,0,0x80,50,
    9,4,0,0,2,0xff,0xff,0xff,0,
    7,5,0x81,3,8,0,10,
    7,5,0x82,2,64,0,0
};
static const uint8_t bos_descriptor[] = {
    5,15,57,0,2,
    28,16,5,0,0xdf,0x60,0xdd,0xd8,0x89,0x45,0xc7,0x4c,
    0x9c,0xd2,0x65,0x9d,0x9e,0x64,0x8a,0x9f,0,0,3,6,30,0,1,0,
    24,16,5,0,0x38,0xb6,0x08,0x34,0xa9,0x09,0xa0,0x47,
    0x8b,0xfd,0xa0,0x76,0x88,0x15,0xb6,0x65,0,1,4,1
};
static const uint8_t msos_descriptor[] = {
    10,0,0,0,0,0,3,6,30,0,
    20,0,3,0,'W','I','N','U','S','B',0,0,0,0,0,0,0,0,0,0
};
static const uint8_t version[] = {2,0,8};

typedef union { uint8_t bytes[DEMO_ROW_BYTES]; uint32_t words[DEMO_ROW_BYTES/4]; } Row;
static Row rows[4];
static uint8_t blue[DEMO_WIDTH];
static uint8_t green[DEMO_HEIGHT];
static uint8_t file_header[DEMO_HEADER_BYTES] __attribute__((aligned(4)));
static volatile uint32_t generated_rows, consumed;
static uint16_t pending_bytes;
static volatile uint8_t start_requested, need_arm, active;
static uint8_t bulk_busy, bulk_toggle;

static uint16_t get16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static void put32(uint8_t *p, uint32_t value)
{
    p[0]=value; p[1]=value>>8; p[2]=value>>16; p[3]=value>>24;
}
static void generate_row(uint32_t row)
{
    uint8_t *dst=rows[row%4].bytes;
    uint32_t actual=DEMO_HEIGHT-1-row;
    uint8_t g=green[actual];
    for (uint32_t col=0; col<DEMO_WIDTH; col++) {
        *dst++=blue[col]; *dst++=g; *dst++=(uint8_t)(col+actual);
    }
}
static void arm_bulk(void)
{
    if (!active || bulk_busy) return;
    const uint8_t *p;
    unsigned length;
    if (consumed>=DEMO_FILE_BYTES) { active=0; return; }
    if (consumed<DEMO_HEADER_BYTES) {
        p=file_header+consumed; length=DEMO_HEADER_BYTES-consumed;
    } else {
        uint32_t pixel=consumed-DEMO_HEADER_BYTES;
        uint32_t row=pixel/DEMO_ROW_BYTES, offset=pixel%DEMO_ROW_BYTES;
        if (row>=generated_rows) { need_arm=1; return; }
        p=rows[row%4].bytes+offset;
        length=DEMO_ROW_BYTES-offset;
    }
    if (length>64) length=64;
    pending_bytes=length; bulk_busy=1;
    board_bulk_tx(p,length,bulk_toggle);
}
void demo_poll(void)
{
    if (start_requested) {
        start_requested=0; consumed=0; generated_rows=0; pending_bytes=0;
        active=1; bulk_busy=0; need_arm=1;
    }
    if (!active) return;
    uint32_t completed_row=consumed<DEMO_HEADER_BYTES ? 0 : (consumed-DEMO_HEADER_BYTES)/DEMO_ROW_BYTES;
    while (generated_rows<completed_row+3 && generated_rows<DEMO_HEIGHT) {
        generate_row(generated_rows);
        __asm volatile("" ::: "memory");
        generated_rows++;
    }
    if (need_arm) { need_arm=0; arm_bulk(); }
}
static void send_next(void)
{
    unsigned length=residue>64 ? 64 : residue;
    board_ep0_tx(reply,length,ep0_toggle);
    reply+=length; residue-=length; ep0_toggle^=1;
}
static void respond(const uint8_t *data, unsigned length, unsigned requested)
{
    residue=length<requested ? length : requested;
    send_zlp=length<requested && !(residue%64);
    reply=data; ep0_toggle=1; send_next();
}
static void ack(void) { board_ep0_tx(demo_ep0,0,1); }
static void stop_file(void)
{
    board_bulk_stop(); active=bulk_busy=start_requested=need_arm=0; pending_bytes=0;
}
void demo_usb_reset(void)
{
    demo_configured=0; residue=send_zlp=address_pending=0;
    stop_file(); bulk_toggle=0;
}
void demo_usb_setup(void)
{
    uint8_t type=demo_ep0[0], request=demo_ep0[1];
    uint16_t value=get16(demo_ep0+2), index=get16(demo_ep0+4), length=get16(demo_ep0+6);
    control_in=type&0x80; residue=send_zlp=address_pending=0;
    if (type==0x80 && request==6) {
        const uint8_t *data=NULL; unsigned size=0;
        switch (value>>8) {
        case 1: data=device_descriptor; size=sizeof(device_descriptor); break;
        case 2: data=config_descriptor; size=sizeof(config_descriptor); break;
        case 15: data=bos_descriptor; size=sizeof(bos_descriptor); break;
        case 3: {
            const char *text=NULL;
            switch (value&255) {
            case 0: scratch[0]=4; scratch[1]=3; scratch[2]=9; scratch[3]=4; size=4; break;
            case 1: text="WCH USB Demo"; break;
            case 2: text="CH32V208WB USBFS J1"; break;
            case 3: text=board_serial(); break;
            }
            if (text) {
                unsigned n=0;
                while (text[n] && n<30) { scratch[2+n*2]=text[n]; scratch[3+n*2]=0; n++; }
                size=2+n*2; scratch[0]=size; scratch[1]=3;
            }
            if (size) data=scratch;
            break;
        }
        }
        if (data) { respond(data,size,length); return; }
    } else if (type==0 && request==5 && value<128 && !index && !length) {
        pending_address=value; address_pending=1; ack(); return;
    } else if (type==0 && request==9 && value<=1 && !index && !length) {
        stop_file(); demo_configured=value; bulk_toggle=0;
        board_endpoints(); ack(); return;
    } else if (type==0x80 && request==8 && !value && !index && length==1) {
        scratch[0]=demo_configured; respond(scratch,1,length); return;
    } else if (type==0x81 && request==10 && !index && !value && length==1) {
        scratch[0]=0; respond(scratch,1,length); return;
    } else if (type==1 && request==11 && !index && !value && !length && demo_configured) {
        ack(); return;
    } else if ((type==0x80 || type==0x81 || type==0x82) && request==0 && !value && length==2) {
        scratch[0]=scratch[1]=0; respond(scratch,2,length); return;
    } else if (type==0x42 || type==2) {
        /* Endpoint halt recovery is supported for the benchmark IN endpoint. */
        if (type==2 && request==1 && !value && index==0x82 && !length) {
            stop_file(); bulk_toggle=0; board_endpoints(); ack(); return;
        }
    } else if (type==0xc0 && request==1 && index==7) {
        respond(msos_descriptor,sizeof(msos_descriptor),length); return;
    } else if (type==0xc0 && request==4 && index==2 && value==1) {
        static const char url[]="fvlb.github.io/webusb-led/";
        scratch[0]=3+sizeof(url)-1; scratch[1]=3; scratch[2]=1;
        memcpy(scratch+3,url,sizeof(url)-1); respond(scratch,scratch[0],length); return;
    } else if (type==0x40 && !index && !length) {
        if (request==2 && value<=255) { demo_brightness=value; ack(); return; }
        if (request==6) { debounce_ms=value; ack(); return; }
        if (request==10 && !value && demo_configured) {
            stop_file(); start_requested=1; ack(); return;
        }
    } else if (type==0xc0 && !index && !value) {
        switch (request) {
        case 3: scratch[0]=demo_brightness; respond(scratch,1,length); return;
        case 5: respond(version,sizeof(version),length); return;
        case 7: scratch[0]=1; respond(scratch,1,length); return; /* no button wired */
        case 8: put32(scratch,board_millis()); respond(scratch,4,length); return;
        case 9: put32(scratch,DEMO_FILE_BYTES); respond(scratch,4,length); return;
        case 12:
            put32(scratch,ep2_completions); put32(scratch+4,demo_usb_irqs);
            put32(scratch+8,board_core_clock()); put32(scratch+12,generated_rows);
            put32(scratch+16,consumed); respond(scratch,20,length); return;
        }
    }
    board_ep0_stall();
}
void demo_usb_in(unsigned endpoint)
{
    if (!endpoint) {
        if (address_pending) { board_address(pending_address); address_pending=0; }
        if (control_in && residue) send_next();
        else if (control_in && send_zlp) { send_zlp=0; send_next(); }
        else board_ep0_rx(1);
    } else if (endpoint==2 && bulk_busy) {
        ep2_completions++; consumed+=pending_bytes; pending_bytes=0;
        bulk_busy=0; bulk_toggle^=1; arm_bulk();
    }
}
void demo_usb_out(unsigned endpoint, unsigned length)
{
    if (endpoint==0 && length==0) board_ep0_rx(1);
    else board_ep0_stall();
}
void demo_init(void)
{
    memset(file_header,0,sizeof(file_header));
    file_header[0]='B'; file_header[1]='M'; put32(file_header+2,DEMO_FILE_BYTES);
    put32(file_header+10,54); put32(file_header+14,40);
    put32(file_header+18,DEMO_WIDTH); put32(file_header+22,DEMO_HEIGHT);
    file_header[26]=1; file_header[28]=24;
    put32(file_header+34,DEMO_ROW_BYTES*DEMO_HEIGHT);
    for (unsigned i=0;i<DEMO_WIDTH;i++) blue[i]=i*255/(DEMO_WIDTH-1);
    for (unsigned i=0;i<DEMO_HEIGHT;i++) green[i]=i*255/(DEMO_HEIGHT-1);
    demo_brightness=0; demo_usb_irqs=ep2_completions=0;
    demo_usb_reset();
}
