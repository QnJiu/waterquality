#include "tp_upgrade_fram.h"
#include "tp_boot.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct { uint8_t mem[2048]; int count, fail; size_t partial; bool read_fail; } fake;
static tp_result wr(void *ctx,uint8_t dev,const uint8_t *tx,size_t n) {
    fake *f=ctx; assert(dev>=0x50 && dev<=0x57 && n>=2 && (size_t)tx[0]+n-1<=256);
    size_t addr=(size_t)(dev-0x50)*256+tx[0];
    if(++f->count==f->fail) { size_t k=f->partial; if(k>n-1) k=n-1; memcpy(f->mem+addr,tx+1,k); return TP_IO_ERROR; }
    memcpy(f->mem+addr,tx+1,n-1); return TP_OK;
}
static tp_result rd(void *ctx,uint8_t dev,const uint8_t *tx,size_t tn,uint8_t *rx,size_t n) {
    fake *f=ctx; if(f->read_fail) return TP_IO_ERROR;
    assert(dev>=0x50 && dev<=0x57 && tn==1 && (size_t)tx[0]+n<=256);
    memcpy(rx,f->mem+(dev-0x50)*256+tx[0],n); return TP_OK;
}
static tp_result verify(void *ctx,uint8_t bank,uint32_t version) {
    (void)ctx; return ((bank==0 && version==1) || (bank==1 && version==2)) ? TP_OK : TP_INVALID;
}
int main(void) {
    fake f={0}; memset(f.mem,0xff,sizeof(f.mem)); tp_i2c_io bus={&f,wr,rd}; tp_upgrade_fram regions;
    assert(tp_upgrade_fram_init(NULL,&bus)==TP_INVALID);
    assert(tp_upgrade_fram_init(&regions,&bus)==TP_OK);
    tp_store_io stores[4]={tp_upgrade_fram_store(&regions.boot),tp_upgrade_fram_store(&regions.download),tp_upgrade_fram_store(&regions.manifests),tp_upgrade_fram_store(&regions.staging)};
    uint8_t data[128], out[128]; memset(data,0x35,sizeof(data));
    for(unsigned i=0;i<4;i++) for(unsigned j=0;j<2;j++) {
        data[0]=(uint8_t)(i*2+j); assert(stores[i].write(stores[i].ctx,j,0,data,sizeof(data))==TP_OK);
        assert(stores[i].read(stores[i].ctx,j,0,out,sizeof(out))==TP_OK && !memcmp(data,out,sizeof(data)));
        assert(stores[i].read(stores[i].ctx,2,0,out,1)==TP_INVALID);
        assert(stores[i].write(stores[i].ctx,j,127,data,2)==TP_INVALID);
    }
    for(unsigned i=0;i<8;i++) assert(f.mem[i*128]==i);
    for(unsigned i=1024;i<2048;i++) assert(f.mem[i]==0xff);
    /* Interrupt every erase transfer, including partial invalidation and partial erase.
       Other slots and regions must never change; an altered slot cannot retain its commit. */
    const unsigned offsets[4]={32,64,116,64}; const uint8_t marks[4][4]={{0x54,0x4d,0x4f,0x43},{0x54,0x4d,0x4f,0x43},{0x54,0x50,0x4d,0x31},{0x54,0x4d,0x4f,0x43}};
    for(unsigned region=0;region<4;region++) for(int at=1;at<=9;at++) for(size_t partial=0;partial<=16;partial++) {
        memset(f.mem,0x35,sizeof(f.mem)); memcpy(f.mem+region*256+offsets[region],marks[region],4);
        uint8_t before[2048]; memcpy(before,f.mem,sizeof(before)); f.count=0; f.fail=at; f.partial=partial;
        assert(stores[region].erase(stores[region].ctx,0)==TP_IO_ERROR);
        assert(!memcmp(f.mem+region*256+128,before+region*256+128,128));
        for(unsigned k=0;k<2048;k++) if(k<region*256 || k>=region*256+128) assert(f.mem[k]==before[k]);
        if(memcmp(f.mem+region*256,before+region*256,128)) assert(memcmp(f.mem+region*256+offsets[region],marks[region],4));
    }
    /* The real boot state machine must recover from interruption in each FRAM write. */
    for(int at=1;at<=15;at++) for(size_t partial=0;partial<=4;partial++) {
        memset(f.mem,0xff,sizeof(f.mem)); f.fail=0; f.count=0;
        tp_boot_io boot={stores[0],NULL,verify}; tp_boot_state state;
        assert(tp_boot_provision(&boot,0,1)==TP_OK);
        f.count=0; f.fail=at; f.partial=partial; tp_result result=tp_boot_stage(&boot,1,2);
        assert(result==TP_IO_ERROR || result==TP_OK); f.fail=0;
        assert(tp_boot_read(&boot,&state)==TP_OK && state.active_bank==0 && state.active_version==1);
        assert(state.phase==TP_BOOT_IDLE || state.phase==TP_BOOT_READY);
    }
    f.read_fail=true; assert(stores[0].read(stores[0].ctx,0,0,out,1)==TP_IO_ERROR);
    puts("upgrade FRAM partition and recovery checks passed"); return 0;
}
