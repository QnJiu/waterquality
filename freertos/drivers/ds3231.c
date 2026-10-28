#include "tp_ds3231.h"
#include <string.h>
#define RTC_ADDRESS 0x68u
#define OSF 0x80u
#define BASE_UNIX 946684800LL
#define END_UNIX 4102444800LL
static bool leap(int y) { return (y%4==0 && (y%100!=0 || y%400==0)); }
static int month_days(int y,int m) {
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[m-1] + (m==2 && leap(y));
}
static int from_bcd(uint8_t b) {
    return (b & 15u)>9u || (b>>4)>9u ? -1 : (int)(b>>4)*10+(int)(b & 15u);
}
static uint8_t bcd(int n) { return (uint8_t)(((n/10)<<4)|(n%10)); }
static tp_result read_reg(tp_ds3231 *r,uint8_t reg,uint8_t *out,size_t n) {
    return r->io.write_read(r->io.ctx,RTC_ADDRESS,&reg,1,out,n);
}
static tp_result decode(const uint8_t *v,int64_t *out) {
    if ((v[0]&0x80u) || (v[1]&0x80u) || (v[2]&0x80u) ||
        (v[3]&0xf8u) || (v[4]&0xc0u) || (v[5]&0xe0u)) return TP_INVALID;
    int sec=from_bcd(v[0]), min=from_bcd(v[1]), hour;
    if (v[2]&0x40u) {
        hour=from_bcd(v[2]&0x1fu);
        if (hour<1 || hour>12) return TP_INVALID;
        hour=hour%12 + ((v[2]&0x20u)?12:0);
    } else hour=from_bcd(v[2]&0x3fu);
    int day=from_bcd(v[4]), month=from_bcd(v[5]), year=from_bcd(v[6]);
    if (sec<0 || sec>59 || min<0 || min>59 || hour<0 || hour>23 ||
        v[3]<1 || v[3]>7 || month<1 || month>12 || year<0 ||
        day<1 || day>month_days(2000+year,month)) return TP_INVALID;
    int64_t days=0;
    for (int y=2000;y<2000+year;y++) days+=leap(y)?366:365;
    for (int m=1;m<month;m++) days+=month_days(2000+year,m);
    days+=day-1;
    *out=BASE_UNIX+days*86400+hour*3600+min*60+sec;
    return TP_OK;
}
tp_result tp_ds3231_init(tp_ds3231 *r,const tp_i2c_io *io) {
    if (!r || !io || !io->write || !io->write_read) return TP_INVALID;
    r->io=*io; return TP_OK;
}
tp_result tp_ds3231_read_utc(tp_ds3231 *r,int64_t *seconds) {
    if (!r || !seconds || !r->io.write_read) return TP_INVALID;
    uint8_t status, a[7], b[7];
    for (int attempt=0;attempt<3;attempt++) {
        tp_result x=read_reg(r,0x0f,&status,1);
        if (x!=TP_OK) return x;
        if (status&OSF) return TP_INVALID;
        x=read_reg(r,0,a,7); if (x!=TP_OK) return x;
        x=read_reg(r,0,b,7); if (x!=TP_OK) return x;
        x=read_reg(r,0x0f,&status,1); if (x!=TP_OK) return x;
        if (status&OSF) return TP_INVALID;
        if (memcmp(a,b,7)==0) return decode(a,seconds);
    }
    return TP_BUSY;
}
tp_result tp_ds3231_set_utc(tp_ds3231 *r,int64_t seconds) {
    if (!r || !r->io.write || !r->io.write_read || seconds<BASE_UNIX || seconds>=END_UNIX) return TP_INVALID;
    int64_t days=(seconds-BASE_UNIX)/86400, rem=(seconds-BASE_UNIX)%86400;
    int year=2000;
    while (days >= (leap(year)?366:365)) { days-=leap(year)?366:365; year++; }
    int month=1;
    while (days>=month_days(year,month)) { days-=month_days(year,month); month++; }
    /* DS3231 day of week is user-defined; 1=Sunday, 2000-01-01 was Saturday. */
    int weekday=(int)(((seconds-BASE_UNIX)/86400+6)%7)+1;
    uint8_t tx[8]={0,bcd((int)(rem%60)),bcd((int)(rem/60%60)),bcd((int)(rem/3600)),
                   (uint8_t)weekday,bcd((int)days+1),bcd(month),bcd(year-2000)};
    tp_result x=r->io.write(r->io.ctx,RTC_ADDRESS,tx,sizeof(tx)); if (x!=TP_OK) return x;
    uint8_t status; x=read_reg(r,0x0f,&status,1); if (x!=TP_OK) return x;
    uint8_t clear[2]={0x0f,(uint8_t)((status & ~OSF) | 0x03u)};
    return r->io.write(r->io.ctx,RTC_ADDRESS,clear,sizeof(clear));
}
