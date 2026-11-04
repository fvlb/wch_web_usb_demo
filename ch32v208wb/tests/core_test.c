#include "demo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t control[64];
static unsigned control_length;
static int stalled;
static const uint8_t *dma;
static unsigned dma_length, dma_busy;
static int dma_toggle;
static uint8_t address;
void board_ep0_tx(const uint8_t *p, unsigned n, int data1)
{
    (void)data1; assert(n<=64);
    if (n) memcpy(control,p,n);
    control_length=n;
}
void board_ep0_rx(int data1) { (void)data1; }
void board_ep0_stall(void) { stalled=1; }
void board_bulk_tx(const uint8_t *p, unsigned n, int data1)
{
    assert(!dma_busy && n && n<=64 && !((uintptr_t)p&3));
    dma=p; dma_length=n; dma_busy=1; dma_toggle=data1;
}
void board_bulk_stop(void) { dma_busy=0; }
void board_address(uint8_t value) { address=value; }
void board_endpoints(void) {}
uint32_t board_millis(void) { return 12345; }
uint32_t board_core_clock(void) { return 96000000; }
const char *board_serial(void) { return "V208-test"; }
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void setup(unsigned type, unsigned request, unsigned value, unsigned index, unsigned n)
{
    uint8_t p[]={type,request,value,value>>8,index,index>>8,n,n>>8};
    memcpy(demo_ep0,p,8); stalled=0; demo_usb_setup();
}
static uint8_t expected(uint32_t position)
{
    uint32_t pixel=position-54, row=pixel/DEMO_ROW_BYTES;
    uint32_t col=(pixel/3)%DEMO_WIDTH, actual=DEMO_HEIGHT-1-row;
    if (pixel%3==0) return col*255/(DEMO_WIDTH-1);
    if (pixel%3==1) return actual*255/(DEMO_HEIGHT-1);
    return (uint8_t)(col+actual);
}
static void image(void)
{
    uint32_t position=0;
    unsigned packet=0;
    int last_toggle=-1;
    while (position<DEMO_FILE_BYTES) {
        if (!dma_busy) demo_poll();
        assert(dma_busy);
        uint8_t snapshot[64]; memcpy(snapshot,dma,dma_length);
        if (packet%3==0) for (unsigned i=0;i<50;i++) demo_poll();
        assert(!memcmp(snapshot,dma,dma_length));
        if (!position) assert(dma_length==54 && dma[0]=='B' && get32(dma+2)==DEMO_FILE_BYTES);
        for (unsigned i=0;i<dma_length;i++) if (position+i>=54) assert(dma[i]==expected(position+i));
        if (last_toggle!=-1) assert(dma_toggle!=last_toggle);
        last_toggle=dma_toggle;
        position+=dma_length; packet++; dma_busy=0; demo_usb_in(2);
    }
    assert(!dma_busy);
}
int main(void)
{
    demo_init();
    setup(0x80,6,0x100,0,18); assert(!stalled && control_length==18 && control[7]==64);
    setup(0x80,6,0x200,0,255); assert(control_length==32 && control[4]==1);
    setup(0x80,6,0xf00,0,255); assert(control_length==57 && control[4]==2);
    setup(0xc0,1,0,7,255); assert(control_length==30 && !stalled);
    setup(0xc0,4,1,2,255); assert(control[2]==1 && !stalled);
    setup(0x40,10,0,0,0); assert(stalled);
    setup(0,5,42,0,0); assert(address==0 && control_length==0); demo_usb_in(0); assert(address==42);
    setup(0,9,1,0,0); assert(!stalled && demo_configured);
    setup(0x40,2,123,0,0); assert(!stalled && demo_brightness==123);
    setup(0xc0,3,0,0,1); assert(control_length==1 && control[0]==123);
    setup(0xc0,8,0,0,4); assert(get32(control)==12345);
    setup(0xc0,9,0,0,4); assert(get32(control)==10484790);
    setup(0x40,10,0,0,0); demo_poll(); image();
    setup(0x40,10,0,0,0); demo_poll();
    assert(dma_busy); setup(0x40,10,0,0,0); demo_poll(); image();
    setup(0,9,0,0,0); assert(!demo_configured && !dma_busy);
    setup(0xc0,0xff,0,0,1); assert(stalled);
    puts("V208 core: descriptors, controls, full 10 MiB images, DMA ownership and restart passed");
}
