/* Physical V208 control/descriptor smoke test; restore LED state on exit. */
#define _POSIX_C_SOURCE 200809L
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}

static int read_control(libusb_device_handle *dev, uint8_t type, uint8_t request,
                        uint16_t value, uint16_t index, uint8_t *data, uint16_t size,
                        int expected)
{
    int rc=libusb_control_transfer(dev,type,request,value,index,data,size,2000);
    if (rc!=expected) {
        fprintf(stderr,"Control %02x/%02x: got %d, expected %d (%s)\n",
                type,request,rc,expected,rc<0 ? libusb_error_name(rc) : "length");
        return -1;
    }
    return 0;
}

int main(void)
{
    libusb_context *ctx=NULL;
    libusb_device_handle *dev=NULL;
    uint8_t data[256], original=0;
    int result=1, restore=0;
    if (libusb_init(&ctx)) return 1;
    dev=libusb_open_device_with_vid_pid(ctx,0x1209,1);
    if (!dev) { fprintf(stderr,"No accessible 1209:0001\n"); goto cleanup; }
    if (read_control(dev,0xc0,5,0,0,data,3,3) || memcmp(data,"\x02\x00\x08",3)) goto cleanup;
    if (read_control(dev,0x80,6,0x0f00,0,data,sizeof(data),57) || data[4]!=2) goto cleanup;
    if (read_control(dev,0xc0,1,0,7,data,sizeof(data),30) || memcmp(data+14,"WINUSB",6)) goto cleanup;
    if (read_control(dev,0xc0,4,1,2,data,sizeof(data),29) || data[2]!=1 ||
        memcmp(data+3,"fvlb.github.io/webusb-led/",26)) goto cleanup;
    if (read_control(dev,0xc0,9,0,0,data,4,4) || get32(data)!=10484790) goto cleanup;
    if (read_control(dev,0xc0,3,0,0,data,1,1)) goto cleanup;
    original=data[0]; restore=1;
    static const uint8_t levels[]={0,64,128,255};
    for (unsigned i=0;i<sizeof(levels);i++) {
        if (read_control(dev,0x40,2,levels[i],0,NULL,0,0) ||
            read_control(dev,0xc0,3,0,0,data,1,1) || data[0]!=levels[i]) goto cleanup;
    }
    if (read_control(dev,0xc0,8,0,0,data,4,4)) goto cleanup;
    uint32_t before=get32(data);
    struct timespec wait={0,100000000};
    nanosleep(&wait,NULL);
    if (read_control(dev,0xc0,8,0,0,data,4,4)) goto cleanup;
    uint32_t elapsed=get32(data)-before;
    if (elapsed<80 || elapsed>500) { fprintf(stderr,"Unexpected timer delta %u\n",elapsed); goto cleanup; }
    if (read_control(dev,0xc0,12,0,0,data,20,20)) goto cleanup;
    printf("Counters: ep2=%u usb_irqs=%u core=%u rows=%u consumed=%u\n",
           get32(data),get32(data+4),get32(data+8),get32(data+12),get32(data+16));
    if (!get32(data+4) || get32(data+8)!=96000000) goto cleanup;
    puts("PASS: V208 version, BOS, WinUSB, WebUSB URL, file size, LED readback, timer, counters");
    result=0;
cleanup:
    if (restore && read_control(dev,0x40,2,original,0,NULL,0,0)) result=1;
    if (dev) libusb_close(dev);
    libusb_exit(ctx);
    return result;
}
