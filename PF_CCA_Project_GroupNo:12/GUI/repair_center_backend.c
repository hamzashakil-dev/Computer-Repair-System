/* Computer Repair Center backend — derived from uploaded repair_center.c.
 * Keeps Ticket/Part layouts and tickets.dat/parts.dat binary record format.
 * Local demo API: sequential Winsock server bound only to IPv4 loopback.
 * Build with Dev-C++: gcc repair_center_backend.c -o repair_center_backend.exe -lws2_32
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define close_socket closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define close_socket close
#endif

#define MAX 100
#define PORT 8765
#define REQ_CAP 65536
#define OUT_CAP 262144
#define TOKEN_CAP 80

typedef struct {
    int id;
    char name[50];
    char phone[20];
    char device[50];
    char issue[100];
    int status;
    int month;
    float cost;
} Ticket;

typedef struct {
    int id;
    char name[50];
    int quantity;
    float price;
} Part;

/* Original functions retained for the original console workflow and logic. */
int getInt(char prompt[], int min, int max);
float getFloat(char prompt[], float min);
void getText(char prompt[], char dest[], int size);
void flushLine(char line[]);
int adminLogin(void);
void userMenu(Ticket t[], int *tc, Part p[], int *pc);
void adminMenu(Ticket t[], int tc, Part p[], int pc);
void registerTicket(Ticket t[], int *tc);
void listTickets(Ticket t[], int tc);
void searchTicket(Ticket t[], int tc);
void sortByCost(Ticket t[], int tc);
void updateStatus(Ticket t[], int tc);
void addPart(Part p[], int *pc);
void listParts(Part p[], int pc);
void showStats(Ticket t[], int tc);
void showGraph(Ticket t[], int tc);
int countByStatus(Ticket t[], int tc, int index, int status);
char *statusName(int status);
void saveData(Ticket t[], int tc, Part p[], int pc);
void loadData(Ticket t[], int *tc, Part p[], int *pc);

static Ticket tickets[MAX];
static Part parts[MAX];
static int ticketCount = 0, partCount = 0;
static char adminToken[TOKEN_CAP] = "";
static int ticketsFileOK = 1, partsFileOK = 1;
static int saveAllChecked(void);
static int saveTicketsChecked(void);
static int savePartsChecked(void);
static unsigned long tokenCounter = 0;

/* A small strict JSON object reader. It accepts string, integer and decimal
   scalar values, decodes JSON escapes/UTF-8 bytes, rejects malformed input,
   and does not depend on third-party libraries. */
typedef struct { const char *p; int bad; } Json;
static void ws(Json *j) { while (*j->p && isspace((unsigned char)*j->p)) j->p++; }
static int jstring(Json *j, char *out, size_t cap) {
    size_t n=0; if (*j->p++!='"') return 0;
    while (*j->p && *j->p!='"') {
        unsigned char c=(unsigned char)*j->p++;
        if(c=='\\') { c=(unsigned char)*j->p++; if(!c) return 0;
            switch(c) { case '"': case '\\': case '/': break; case 'b':c='\b';break; case 'f':c='\f';break; case 'n':c='\n';break; case 'r':c='\r';break; case 't':c='\t';break;
                case 'u': { int k, v=0; for(k=0;k<4;k++){int h=(unsigned char)*j->p++; if(!h)return 0; if(h>='0'&&h<='9')v=v*16+h-'0';else if(h>='a'&&h<='f')v=v*16+h-'a'+10;else if(h>='A'&&h<='F')v=v*16+h-'A'+10;else return 0;}
                    if(v<128)c=(unsigned char)v; else if(v<2048){ if(n+2>=cap)return 0; out[n++]=(char)(0xC0|(v>>6)); c=(unsigned char)(0x80|(v&63));} else {if(n+3>=cap)return 0;out[n++]=(char)(0xE0|(v>>12));out[n++]=(char)(0x80|((v>>6)&63));c=(unsigned char)(0x80|(v&63));} break; }
                default:return 0;
            }
        }
        if(c<32 || n+1>=cap) return 0;
        out[n++]=(char)c;
    }
    if(*j->p++!='"') return 0;
    out[n]=0; return 1;
}
static int jskip(Json *j) { char tmp[2048]; ws(j); if(*j->p=='"') return jstring(j,tmp,sizeof(tmp));
    if(*j->p=='{'||*j->p=='['||!*j->p)return 0;
    {const char *s=j->p; while(*j->p && *j->p!=',' && *j->p!='}' && !isspace((unsigned char)*j->p))j->p++; return j->p>s;} }
static int jsonGet(const char *src,const char *key,char *out,size_t cap) {
    Json j; char k[128]; int found=0; j.p=src; j.bad=0; out[0]=0; ws(&j); if(*j.p++!='{')return 0; ws(&j);
    if(*j.p=='}')return 0;
    while(*j.p && *j.p!='}') { if(!jstring(&j,k,sizeof(k)))return 0; ws(&j); if(*j.p++!=':')return 0; ws(&j);
        if(strcmp(k,key)==0) { if(found)return 0; if(*j.p=='"'){if(!jstring(&j,out,cap))return 0;}else{size_t n=0;while(j.p[n]&&j.p[n]!=','&&j.p[n]!='}'&&!isspace((unsigned char)j.p[n]))n++;if(n==0||n>=cap)return 0;memcpy(out,j.p,n);out[n]=0;j.p+=n;} found=1; }
        else if(!jskip(&j))return 0;
        ws(&j); if(*j.p==','){j.p++;ws(&j);if(*j.p=='}')return 0;}else if(*j.p!='}')return 0;
    }
    if(*j.p++!='}')return 0;
    ws(&j);return found&&*j.p==0;
}
static int parseInt(const char *s,int *v) { char *e; long x; if(!s||!*s)return 0; errno=0;x=strtol(s,&e,10);if(errno||*e||x<-2147483647L||x>2147483647L)return 0;*v=(int)x;return 1; }
static int parseFloat(const char *s,float *v) { char *e; double x;if(!s||!*s)return 0;errno=0;x=strtod(s,&e);if(errno||*e||x!=x||x>1000000.0||x<0)return 0;*v=(float)x;return 1; }
static int safeText(const char *s,size_t cap,int required) { size_t n=strlen(s),i;if((required&&n==0)||n>=cap)return 0;for(i=0;i<n;i++)if((unsigned char)s[i]<32)return 0;return 1; }
static void jsonEscape(char *out,size_t cap,const char *s) { size_t n=0;while(*s&&n+7<cap){unsigned char c=(unsigned char)*s++;if(c=='"'||c=='\\'){out[n++]='\\';out[n++]=(char)c;}else if(c<32){int z=snprintf(out+n,cap-n,"\\u%04x",c);if(z<0)break;n+=(size_t)z;}else out[n++]=(char)c;}out[n]=0; }
static void add(char *out,size_t cap,size_t *n,const char *fmt,...) { va_list ap;int z;if(*n>=cap)return;va_start(ap,fmt);z=vsnprintf(out+*n,cap-*n,fmt,ap);va_end(ap);if(z>0){if((size_t)z>=cap-*n)*n=cap-1;else *n+=(size_t)z;} }
static int findTicket(int id){int i;for(i=0;i<ticketCount;i++)if(tickets[i].id==id)return i;return -1;}
static void ticketJson(char *o,size_t cap,size_t *n,const Ticket *t){char a[256],b[128],c[256],d[512];jsonEscape(a,sizeof(a),t->name);jsonEscape(b,sizeof(b),t->phone);jsonEscape(c,sizeof(c),t->device);jsonEscape(d,sizeof(d),t->issue);add(o,cap,n,"{\"id\":%d,\"name\":\"%s\",\"phone\":\"%s\",\"device\":\"%s\",\"issue\":\"%s\",\"status\":%d,\"statusName\":\"%s\",\"month\":%d,\"cost\":%.2f}",t->id,a,b,c,d,t->status,statusName(t->status),t->month,t->cost);}
static void partJson(char *o,size_t cap,size_t *n,const Part *p){char a[256];jsonEscape(a,sizeof(a),p->name);add(o,cap,n,"{\"id\":%d,\"name\":\"%s\",\"quantity\":%d,\"price\":%.2f}",p->id,a,p->quantity,p->price);}
static void sendAll(SOCKET s,const char *data,int len){int sent=0;while(sent<len){int n=send(s,data+sent,len-sent,0);if(n<=0)return;sent+=n;}}
static void reply(SOCKET s,int code,const char *reason,const char *msg);
static void responseType(SOCKET s,int code,const char *reason,const char *type,const char *body,int n){char h[512];int z=snprintf(h,sizeof(h),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, X-Admin-Token\r\nAccess-Control-Max-Age: 600\r\nCache-Control: no-store\r\n\r\n",code,reason,type,n);sendAll(s,h,z);if(n)sendAll(s,body,n);}
static void response(SOCKET s,int code,const char *reason,const char *body){responseType(s,code,reason,"application/json; charset=utf-8",body,(int)strlen(body));}
static void serveFrontend(SOCKET s){FILE *f;long size;char *html;f=fopen("Computer_Repair_Center.html","rb");if(!f){reply(s,404,"Not Found","Computer_Repair_Center.html is missing from the backend working folder");return;}if(fseek(f,0,SEEK_END)!=0||(size=ftell(f))<0||size>1000000||fseek(f,0,SEEK_SET)!=0){fclose(f);reply(s,500,"Internal Server Error","Could not read frontend file");return;}html=(char*)malloc((size_t)size+1);if(!html){fclose(f);reply(s,500,"Internal Server Error","Not enough memory");return;}if(fread(html,1,(size_t)size,f)!=(size_t)size){free(html);fclose(f);reply(s,500,"Internal Server Error","Could not read frontend file");return;}fclose(f);responseType(s,200,"OK","text/html; charset=utf-8",html,(int)size);free(html);}
static void reply(SOCKET s,int code,const char *reason,const char *msg){char b[2048],e[1536];jsonEscape(e,sizeof(e),msg);snprintf(b,sizeof(b),"{\"error\":\"%s\"}",e);response(s,code,reason,b);}
static int adminOK(const char *headers){char *p=strstr((char*)headers,"X-Admin-Token:");char got[TOKEN_CAP];size_t n;if(!p)return 0;p+=14;while(*p==' '||*p=='\t')p++;n=0;while(p[n]&&p[n]!='\r'&&p[n]!='\n'&&n<TOKEN_CAP-1)n++;memcpy(got,p,n);got[n]=0;return adminToken[0]&&strcmp(got,adminToken)==0;}
static int originAllowed(const char *headers){
#ifdef ARENA_PREVIEW_MODE
    (void)headers; return 1;
#else
    const char *p=strstr(headers,"Origin:");char origin[256];size_t n;if(!p)return 1;p+=7;while(*p==' '||*p=='\t')p++;n=0;while(p[n]&&p[n]!='\r'&&p[n]!='\n'&&n<sizeof(origin)-1)n++;memcpy(origin,p,n);origin[n]=0;if(strcmp(origin,"null")==0||strcmp(origin,"file://")==0||strcmp(origin,"http://127.0.0.1")==0)return 1;if(strncmp(origin,"http://127.0.0.1:",17)==0){size_t i=17;if(!isdigit((unsigned char)origin[i]))return 0;for(;origin[i];i++)if(!isdigit((unsigned char)origin[i]))return 0;return 1;}return 0;
#endif
}
static void genToken(void){unsigned long a,b;int i;srand((unsigned int)(time(NULL)^(unsigned long)clock()^(unsigned long)(uintptr_t)&a));tokenCounter++;for(i=0;i<4;i++){a=((unsigned long)rand()<<16)^(unsigned long)rand()^(unsigned long)time(NULL)^tokenCounter;b=((unsigned long)rand()<<16)^(unsigned long)rand()^(unsigned long)(uintptr_t)&b;snprintf(adminToken+i*16,17,"%08lx%08lx",a&0xffffffffUL,b&0xffffffffUL);}}
static void listTicketsJson(SOCKET s,int sorted,int desc){char *o=(char*)malloc(OUT_CAP);size_t n=0;int idx[MAX],i,j,x;if(!o){reply(s,500,"Internal Server Error","Not enough memory");return;}for(i=0;i<ticketCount;i++)idx[i]=i;if(sorted)for(i=0;i<ticketCount;i++)for(j=0;j<ticketCount-1-i;j++)if((desc?tickets[idx[j]].cost<tickets[idx[j+1]].cost:tickets[idx[j]].cost>tickets[idx[j+1]].cost)){x=idx[j];idx[j]=idx[j+1];idx[j+1]=x;}add(o,OUT_CAP,&n,"{\"tickets\":[");for(i=0;i<ticketCount;i++){if(i)add(o,OUT_CAP,&n,",");ticketJson(o,OUT_CAP,&n,&tickets[idx[i]]);}add(o,OUT_CAP,&n,"]}");response(s,200,"OK",o);free(o);}
static void endpoint(SOCKET s,const char *method,const char *path,const char *headers,const char *body){char out[OUT_CAP],v[2048],v2[2048];size_t n;int i,id,st,month,qty,desc=0;float cost,price;char *q;
    if(strcmp(method,"OPTIONS")==0){response(s,204,"No Content","");return;}
    if(strcmp(path,"/api/health")==0&&strcmp(method,"GET")==0){snprintf(out,sizeof(out),"{\"status\":\"online\",\"application\":\"Computer Repair Center\",\"version\":\"1.0.0\",\"data\":{\"tickets\":\"%s\",\"parts\":\"%s\"},\"ticketCount\":%d,\"partCount\":%d}",ticketsFileOK?"ok":"error",partsFileOK?"ok":"error",ticketCount,partCount);response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/dashboard")==0&&strcmp(method,"GET")==0){int c[4]={0};double totals[4]={0};for(i=0;i<ticketCount;i++){if(tickets[i].status>=0&&tickets[i].status<4){c[tickets[i].status]++;totals[tickets[i].status]+=tickets[i].cost;}}snprintf(out,sizeof(out),"{\"total\":%d,\"received\":%d,\"repairing\":%d,\"ready\":%d,\"collected\":%d,\"parts\":%d}",ticketCount,c[0],c[1],c[2],c[3],partCount);response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/admin/login")==0&&strcmp(method,"POST")==0){char u[128],p[128];if(!jsonGet(body,"username",u,sizeof(u))||!jsonGet(body,"password",p,sizeof(p))){reply(s,400,"Bad Request","Username and password are required");return;}if(strcmp(u,"admin567")||strcmp(p,"admin123")){reply(s,401,"Unauthorized","Incorrect username or password");return;}genToken();snprintf(out,sizeof(out),"{\"token\":\"%s\",\"authenticated\":true}",adminToken);response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/admin/logout")==0&&strcmp(method,"POST")==0){if(!adminOK(headers)){reply(s,401,"Unauthorized","Administrator authentication required");return;}adminToken[0]=0;response(s,200,"OK","{\"ok\":true}");return;}
    if(strncmp(path,"/api/admin/",11)==0&&!adminOK(headers)){reply(s,401,"Unauthorized","Administrator authentication required");return;}
    if(strcmp(path,"/api/tickets")==0&&strcmp(method,"GET")==0){listTicketsJson(s,0,0);return;}
    if(strncmp(path,"/api/tickets/search?",20)==0&&strcmp(method,"GET")==0){q=strstr(path,"id=");if(!q||!parseInt(q+3,&id)){reply(s,400,"Bad Request","Provide a valid ticket ID");return;}i=findTicket(id);if(i<0){reply(s,404,"Not Found","Ticket not found");return;}n=0;out[0]=0;ticketJson(out,sizeof(out),&n,&tickets[i]);response(s,200,"OK",out);return;}
    if(strncmp(path,"/api/tickets/sort?",18)==0&&strcmp(method,"GET")==0){q=strstr(path,"order=");desc=q&&strncmp(q+6,"desc",4)==0;listTicketsJson(s,1,desc);return;}
    if(strcmp(path,"/api/tickets")==0&&strcmp(method,"POST")==0){Ticket t;int found;char phone[64];if(ticketCount>=MAX){reply(s,409,"Conflict","Ticket capacity reached");return;}if(!jsonGet(body,"name",t.name,sizeof(t.name))||!safeText(t.name,sizeof(t.name),1)||!jsonGet(body,"phone",phone,sizeof(phone))||!safeText(phone,sizeof(phone),1)||strlen(phone)>=sizeof(t.phone)||!jsonGet(body,"device",t.device,sizeof(t.device))||!safeText(t.device,sizeof(t.device),1)||!jsonGet(body,"issue",t.issue,sizeof(t.issue))||!safeText(t.issue,sizeof(t.issue),1)||!jsonGet(body,"month",v,sizeof(v))||!parseInt(v,&month)||month<1||month>12||!jsonGet(body,"cost",v,sizeof(v))||!parseFloat(v,&cost)){reply(s,400,"Bad Request","Check required fields, month (1–12), and cost (0–1000000)");return;}for(i=0;phone[i];i++)if(!isdigit((unsigned char)phone[i])){reply(s,400,"Bad Request","Phone number must contain digits only");return;}strcpy(t.phone,phone);t.month=month;t.cost=cost;t.status=0;t.id=101+ticketCount;while((found=findTicket(t.id))>=0)t.id++;tickets[ticketCount++]=t;if(!saveTicketsChecked()){ticketCount--;ticketsFileOK=0;reply(s,500,"Internal Server Error","Could not save ticket data");return;}n=0;out[0]=0;add(out,sizeof(out),&n,"{\"ok\":true,\"ticket\":");ticketJson(out,sizeof(out),&n,&tickets[ticketCount-1]);add(out,sizeof(out),&n,"}");response(s,201,"Created",out);return;}
    if(strncmp(path,"/api/tickets/",13)==0&&strcmp(method,"GET")==0){if(!parseInt(path+13,&id)){reply(s,400,"Bad Request","Invalid ticket ID");return;}i=findTicket(id);if(i<0){reply(s,404,"Not Found","Ticket not found");return;}n=0;out[0]=0;ticketJson(out,sizeof(out),&n,&tickets[i]);response(s,200,"OK",out);return;}
    if(strncmp(path,"/api/tickets/",13)==0&&strstr(path,"/status")&&strcmp(method,"POST")==0){char *p=(char*)path+13;char idbuf[32];size_t k=0;while(p[k]&&p[k]!='/'&&k<sizeof(idbuf)-1)k++;memcpy(idbuf,p,k);idbuf[k]=0;if(!parseInt(idbuf,&id)||!jsonGet(body,"status",v,sizeof(v))||!parseInt(v,&st)||st<0||st>3){reply(s,400,"Bad Request","Status must be 0, 1, 2, or 3");return;}i=findTicket(id);if(i<0){reply(s,404,"Not Found","Ticket not found");return;}tickets[i].status=st;if(!saveTicketsChecked()){ticketsFileOK=0;reply(s,500,"Internal Server Error","Could not save ticket status");return;}snprintf(out,sizeof(out),"{\"ok\":true,\"id\":%d,\"status\":%d,\"statusName\":\"%s\"}",id,st,statusName(st));response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/parts")==0&&strcmp(method,"GET")==0){n=0;out[0]=0;add(out,sizeof(out),&n,"{\"parts\":[");for(i=0;i<partCount;i++){if(i)add(out,sizeof(out),&n,",");partJson(out,sizeof(out),&n,&parts[i]);}add(out,sizeof(out),&n,"]}");response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/parts")==0&&strcmp(method,"POST")==0){Part t;if(partCount>=MAX){reply(s,409,"Conflict","Inventory capacity reached");return;}if(!jsonGet(body,"name",t.name,sizeof(t.name))||!safeText(t.name,sizeof(t.name),1)||!jsonGet(body,"quantity",v,sizeof(v))||!parseInt(v,&qty)||qty<0||qty>100000||!jsonGet(body,"price",v2,sizeof(v2))||!parseFloat(v2,&price)){reply(s,400,"Bad Request","Check part name, quantity (0–100000), and price");return;}for(i=0;i<partCount;i++)if(strcmp(parts[i].name,t.name)==0){reply(s,409,"Conflict","This part already exists");return;}t.id=partCount+1;t.quantity=qty;t.price=price;parts[partCount++]=t;if(!savePartsChecked()){partCount--;partsFileOK=0;reply(s,500,"Internal Server Error","Could not save inventory");return;}n=0;out[0]=0;add(out,sizeof(out),&n,"{\"ok\":true,\"part\":");partJson(out,sizeof(out),&n,&parts[partCount-1]);add(out,sizeof(out),&n,"}");response(s,201,"Created",out);return;}
    if(strcmp(path,"/api/admin/stats")==0&&strcmp(method,"GET")==0){int c[4]={0};double total[4]={0};double revenue=0;for(i=0;i<ticketCount;i++){int z=tickets[i].status;if(z>=0&&z<4){c[z]++;total[z]+=tickets[i].cost;if(z==3)revenue+=tickets[i].cost;}}snprintf(out,sizeof(out),"{\"total\":%d,\"statuses\":[{\"status\":0,\"name\":\"Received\",\"count\":%d,\"costTotal\":%.2f},{\"status\":1,\"name\":\"Repairing\",\"count\":%d,\"costTotal\":%.2f},{\"status\":2,\"name\":\"Ready\",\"count\":%d,\"costTotal\":%.2f},{\"status\":3,\"name\":\"Collected\",\"count\":%d,\"costTotal\":%.2f}],\"revenue\":%.2f}",ticketCount,c[0],total[0],c[1],total[1],c[2],total[2],c[3],total[3],revenue);response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/admin/monthly")==0&&strcmp(method,"GET")==0){int m[12]={0};for(i=0;i<ticketCount;i++)if(tickets[i].month>=1&&tickets[i].month<=12)m[tickets[i].month-1]++;snprintf(out,sizeof(out),"{\"months\":[");n=strlen(out);for(i=0;i<12;i++)add(out,sizeof(out),&n,"%s%d",i?",":"",m[i]);add(out,sizeof(out),&n,"]}");response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/admin/inventory")==0&&strcmp(method,"GET")==0){long units=0;double value=0;for(i=0;i<partCount;i++){units+=parts[i].quantity;value+=(double)parts[i].quantity*parts[i].price;}snprintf(out,sizeof(out),"{\"partTypes\":%d,\"units\":%ld,\"inventoryValue\":%.2f,\"parts\":[",partCount,units,value);n=strlen(out);for(i=0;i<partCount;i++){if(i)add(out,sizeof(out),&n,",");partJson(out,sizeof(out),&n,&parts[i]);}add(out,sizeof(out),&n,"]}");response(s,200,"OK",out);return;}
    if(strcmp(path,"/api/admin/backup")==0&&strcmp(method,"GET")==0){FILE *f;unsigned char tb[sizeof(Ticket)*MAX],pb[sizeof(Part)*MAX];size_t tz=0,pz=0;char *t64,*p64;static const char tab[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t j,k;unsigned int x;f=fopen("tickets.dat","rb");if(f){tz=fread(tb,1,sizeof(tb),f);if(ferror(f)){fclose(f);reply(s,500,"Internal Server Error","Could not read tickets.dat");return;}fclose(f);}f=fopen("parts.dat","rb");if(f){pz=fread(pb,1,sizeof(pb),f);if(ferror(f)){fclose(f);reply(s,500,"Internal Server Error","Could not read parts.dat");return;}fclose(f);}t64=(char*)malloc(tz*2+8);p64=(char*)malloc(pz*2+8);if(!t64||!p64){free(t64);free(p64);reply(s,500,"Internal Server Error","Not enough memory");return;}k=0;for(j=0;j<tz;j+=3){x=(unsigned int)tb[j]<<16;if(j+1<tz)x|=(unsigned int)tb[j+1]<<8;if(j+2<tz)x|=tb[j+2];t64[k++]=tab[(x>>18)&63];t64[k++]=tab[(x>>12)&63];t64[k++]=j+1<tz?tab[(x>>6)&63]:'=';t64[k++]=j+2<tz?tab[x&63]:'=';}t64[k]=0;k=0;for(j=0;j<pz;j+=3){x=(unsigned int)pb[j]<<16;if(j+1<pz)x|=(unsigned int)pb[j+1]<<8;if(j+2<pz)x|=pb[j+2];p64[k++]=tab[(x>>18)&63];p64[k++]=tab[(x>>12)&63];p64[k++]=j+1<pz?tab[(x>>6)&63]:'=';p64[k++]=j+2<pz?tab[x&63]:'=';}p64[k]=0;snprintf(out,sizeof(out),"{\"ticketsDatBase64\":\"%s\",\"partsDatBase64\":\"%s\",\"note\":\"Read-only snapshots of the original binary data files.\"}",t64,p64);free(t64);free(p64);response(s,200,"OK",out);return;}
    reply(s,404,"Not Found","API endpoint not found");
}
static void serveOne(SOCKET client){char req[REQ_CAP+1],headers[REQ_CAP+1],method[16],path[512];int used=0,headerEnd=-1,contentLength=0;char *body,*p;int got;
#ifdef _WIN32
    {DWORD timeout=5000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));}
#else
    {struct timeval timeout;timeout.tv_sec=5;timeout.tv_usec=0;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));}
#endif
    while(used<REQ_CAP){got=recv(client,req+used,REQ_CAP-used,0);if(got<=0){close_socket(client);return;}used+=got;req[used]=0;if(headerEnd<0){p=strstr(req,"\r\n\r\n");if(p){headerEnd=(int)(p-req)+4;break;}}}
    if(headerEnd<0){reply(client,400,"Bad Request","Malformed or oversized HTTP headers");close_socket(client);return;}
    memcpy(headers,req,(size_t)headerEnd);headers[headerEnd]=0;
    if(sscanf(req,"%15s %511s",method,path)!=2){reply(client,400,"Bad Request","Malformed request line");close_socket(client);return;}
    p=strstr(headers,"Content-Length:");if(!p)p=strstr(headers,"content-length:");if(p){long z=strtol(p+15,NULL,10);if(z<0||z>16000){reply(client,413,"Payload Too Large","Request body exceeds 16 KB");close_socket(client);return;}contentLength=(int)z;}
    while(used-headerEnd<contentLength&&used<REQ_CAP){got=recv(client,req+used,REQ_CAP-used,0);if(got<=0)break;used+=got;req[used]=0;}
    if(used-headerEnd<contentLength){reply(client,400,"Bad Request","Incomplete request body");close_socket(client);return;}
    body=req+headerEnd;body[contentLength]=0;
    if(!originAllowed(headers)){reply(client,403,"Forbidden","Origin is not allowed for the loopback service");close_socket(client);return;}
    if(strcmp(method,"GET")&&strcmp(method,"POST")&&strcmp(method,"OPTIONS")){reply(client,405,"Method Not Allowed","Only GET, POST, and OPTIONS are supported");close_socket(client);return;}
    /* Strip query for routing except the two supported query endpoints. */
    if(strncmp(path,"/api/tickets/search?",20)!=0&&strncmp(path,"/api/tickets/sort?",18)!=0){p=strchr(path,'?');if(p)*p=0;}
    /* Re-read the existing files for each request so external edits and all
       app clients observe the same source of truth. The server is sequential,
       so these arrays and file writes cannot race one another. */
    if(strcmp(method,"OPTIONS")!=0) loadData(tickets,&ticketCount,parts,&partCount);
    if(strcmp(method,"GET")==0&&(strcmp(path,"/")==0||strcmp(path,"/Computer_Repair_Center.html")==0)){serveFrontend(client);close_socket(client);return;}
    endpoint(client,method,path,headers,body);close_socket(client);
}
static int startServer(void){SOCKET listenSock,client;struct sockaddr_in addr;
#ifdef _WIN32
    WSADATA wd;if(WSAStartup(MAKEWORD(2,2),&wd)!=0){fprintf(stderr,"Winsock initialization failed.\n");return 1;}
#endif
    listenSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listenSock==INVALID_SOCKET){fprintf(stderr,"Could not create socket.\n");return 1;}{int yes=1;setsockopt(listenSock,SOL_SOCKET,SO_REUSEADDR,(const char*)&yes,sizeof(yes));}memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_port=htons(PORT);
#ifdef ARENA_PREVIEW_MODE
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
#else
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#endif
    if(bind(listenSock,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){fprintf(stderr,"Could not bind 127.0.0.1:%d (port may already be in use).\n",PORT);close_socket(listenSock);return 1;}if(listen(listenSock,16)==SOCKET_ERROR){fprintf(stderr,"Could not listen.\n");close_socket(listenSock);return 1;}
    #ifdef ARENA_PREVIEW_MODE
    printf("Computer Repair Center C backend 1.0.0 [ARENA PREVIEW MODE]\nPreview listener on port %d\nData files: tickets.dat and parts.dat in this folder\nPress Ctrl+C to stop.\n",PORT);
#else
    printf("Computer Repair Center C backend 1.0.0\nListening only on http://127.0.0.1:%d\nData files: tickets.dat and parts.dat in this folder\nPress Ctrl+C to stop.\n",PORT);
#endif
    fflush(stdout);
    for(;;){client=accept(listenSock,NULL,NULL);if(client==INVALID_SOCKET)continue;serveOne(client);}
}
int main(void){loadData(tickets,&ticketCount,parts,&partCount);return startServer();}

static int saveOne(const char *path,const void *data,size_t size,size_t count){char tmp[128];FILE *f;int ok;snprintf(tmp,sizeof(tmp),"%s.tmp",path);f=fopen(tmp,"wb");if(!f)return 0;ok=(fwrite(data,size,count,f)==count&&fflush(f)==0);if(fclose(f)!=0)ok=0;if(!ok){remove(tmp);return 0;}
#ifdef _WIN32
    if(!MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){remove(tmp);return 0;}
#else
    if(rename(tmp,path)!=0){remove(tmp);return 0;}
#endif
    return 1;
}
static int saveAllChecked(void){int a=saveOne("tickets.dat",tickets,sizeof(Ticket),(size_t)ticketCount);int b=saveOne("parts.dat",parts,sizeof(Part),(size_t)partCount);ticketsFileOK=a;partsFileOK=b;return a&&b;}
static int saveTicketsChecked(void){ticketsFileOK=saveOne("tickets.dat",tickets,sizeof(Ticket),(size_t)ticketCount);return ticketsFileOK;}
static int savePartsChecked(void){partsFileOK=saveOne("parts.dat",parts,sizeof(Part),(size_t)partCount);return partsFileOK;}

void flushLine(char line[]) {
    int c;
    if (strchr(line, '\n') == NULL) {
        c = getchar();
        while (c != '\n' && c != EOF) {
            c = getchar();
        }
    }
}

int getInt(char prompt[], int min, int max) {
    char line[100];
    char extra;
    int value;

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 100, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);

        if (sscanf(line, "%d %c", &value, &extra) == 1 && value >= min && value <= max) {
            return value;
        }
        printf("Invalid input! Enter a whole number from %d to %d.\n", min, max);
    }
}

float getFloat(char prompt[], float min) {
    char line[100];
    char extra;
    float value;

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 100, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);
        if (sscanf(line, "%f %c", &value, &extra) == 1 && value >= min && value <= 1000000) {
            return value;
        }
        printf("Invalid input! Enter a number from %.0f to 1000000.\n", min);
    }
}

void getText(char prompt[], char dest[], int size) {
    char line[200];

    while (1) {
        printf("%s", prompt);
        if (fgets(line, 200, stdin) == NULL) {
            exit(0);
        }
        flushLine(line);
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) {
            printf("Invalid input! This cannot be empty.\n");
        } else if ((int)strlen(line) >= size) {
            printf("Invalid input! Maximum %d characters.\n", size - 1);
        } else {
            strcpy(dest, line);
            return;
        }
    }
}

int adminLogin(void) {
    char username[30];
    char password[30];

    printf("\n--- ADMIN LOGIN ---\n");
    getText("Username: ", username, 30);
    getText("Password: ", password, 30);

    if (strcmp(username, "admin567") == 0 && strcmp(password, "admin123") == 0) {
        return 1;
    }
    return 0;
}

void userMenu(Ticket t[], int *tc, Part p[], int *pc) {
    int choice;

    do {
        printf("\n--- USER MENU ---\n");
        printf("1. Register repair ticket\n");
        printf("2. List all tickets\n");
        printf("3. Search ticket by ID\n");
        printf("4. Sort tickets by cost\n");
        printf("5. Update ticket status\n");
        printf("6. Add spare part\n");
        printf("7. List spare parts\n");
        printf("0. Back\n");
        choice = getInt("Enter choice: ", 0, 7);

        switch (choice) {
            case 1: registerTicket(t, tc); saveData(t, *tc, p, *pc); break;
            case 2: listTickets(t, *tc); break;
            case 3: searchTicket(t, *tc); break;
            case 4: sortByCost(t, *tc); saveData(t, *tc, p, *pc); break;
            case 5: updateStatus(t, *tc); saveData(t, *tc, p, *pc); break;
            case 6: addPart(p, pc); saveData(t, *tc, p, *pc); break;
            case 7: listParts(p, *pc); break;
            case 0: break;
        }
    } while (choice != 0);
}

void adminMenu(Ticket t[], int tc, Part p[], int pc) {
    int choice;

    do {
        printf("\n--- ADMIN MENU ---\n");
        printf("1. View all tickets\n");
        printf("2. Statistics and revenue\n");
        printf("3. Monthly repair graph\n");
        printf("4. Inventory summary\n");
        printf("0. Logout\n");
        choice = getInt("Enter choice: ", 0, 4);

        switch (choice) {
            case 1: listTickets(t, tc); break;
            case 2: showStats(t, tc); break;
            case 3: showGraph(t, tc); break;
            case 4: listParts(p, pc); break;
            case 0: printf("Logged out.\n"); break;
        }
    } while (choice != 0);
}

void registerTicket(Ticket t[], int *tc) {
    Ticket n;
    int i, ok;

    if (*tc >= MAX) {
        printf("Ticket list is full (%d tickets).\n", MAX);
        return;
    }

    n.id = 101 + *tc;
    getText("Customer name: ", n.name, 50);
    do {
        ok = 1;
        getText("Phone number (digits only): ", n.phone, 20);
        for (i = 0; n.phone[i] != '\0'; i++) {
            if (n.phone[i] < '0' || n.phone[i] > '9') {
                ok = 0;
            }
        }
        if (ok == 0) {
            printf("Invalid input! Use digits only.\n");
        }
    } while (ok == 0);
    getText("Device (e.g. Laptop): ", n.device, 50);
    getText("Issue description: ", n.issue, 100);
    n.month = getInt("Month (1-12): ", 1, 12);
    n.cost = getFloat("Estimated cost: ", 0);
    n.status = 0;

    t[*tc] = n;
    (*tc)++;
    printf("Ticket registered! Ticket ID = %d\n", n.id);
}

void listTickets(Ticket t[], int tc) {
    int i;

    if (tc == 0) {
        printf("No tickets found.\n");
        return;
    }
    printf("\n--- ALL TICKETS ---\n");
    for (i = 0; i < tc; i++) {
        printf("ID: %d | %s | %s | Month: %d | %s | Cost: %.2f\n",
               t[i].id, t[i].name, t[i].device, t[i].month,
               statusName(t[i].status), t[i].cost);
    }
}

void searchTicket(Ticket t[], int tc) {
    int id, i;

    id = getInt("Enter ticket ID: ", 1, 100000);
    for (i = 0; i < tc; i++) {
        if (t[i].id == id) {
            printf("\n--- TICKET FOUND ---\n");
            printf("ID     : %d\n", t[i].id);
            printf("Name   : %s\n", t[i].name);
            printf("Phone  : %s\n", t[i].phone);
            printf("Device : %s\n", t[i].device);
            printf("Issue  : %s\n", t[i].issue);
            printf("Month  : %d\n", t[i].month);
            printf("Status : %s\n", statusName(t[i].status));
            printf("Cost   : %.2f\n", t[i].cost);
            return;
        }
    }
    printf("Ticket %d not found.\n", id);
}

void sortByCost(Ticket t[], int tc) {
    int i, j;
    Ticket temp;

    if (tc < 2) {
        printf("Not enough tickets to sort.\n");
        return;
    }
    for (i = 0; i < tc - 1; i++) {
        for (j = 0; j < tc - i - 1; j++) {
            if (t[j].cost > t[j + 1].cost) {
                temp = t[j];
                t[j] = t[j + 1];
                t[j + 1] = temp;
            }
        }
    }
    printf("Tickets sorted by cost (lowest first).\n");
    listTickets(t, tc);
}

void updateStatus(Ticket t[], int tc) {
    int id, i;

    id = getInt("Enter ticket ID: ", 1, 100000);
    for (i = 0; i < tc; i++) {
        if (t[i].id == id) {
            t[i].status = getInt("New status (0 Received, 1 Repairing, 2 Ready, 3 Collected): ", 0, 3);
            printf("Status updated to %s.\n", statusName(t[i].status));
            return;
        }
    }
    printf("Ticket %d not found.\n", id);
}

char *statusName(int status) {
    switch (status) {
        case 0: return "Received";
        case 1: return "Repairing";
        case 2: return "Ready";
        case 3: return "Collected";
    }
    return "Unknown";
}

void addPart(Part p[], int *pc) {
    Part n;
    int i;

    if (*pc >= MAX) {
        printf("Inventory is full (%d parts).\n", MAX);
        return;
    }
    n.id = *pc + 1;
    getText("Part name: ", n.name, 50);
    for (i = 0; i < *pc; i++) {
        if (strcmp(p[i].name, n.name) == 0) {
            printf("This part already exists. Duplicate not added.\n");
            return;
        }
    }
    n.quantity = getInt("Quantity: ", 0, 100000);
    n.price = getFloat("Unit price: ", 0);

    p[*pc] = n;
    (*pc)++;
    printf("Spare part added!\n");
}

void listParts(Part p[], int pc) {
    int i;

    if (pc == 0) {
        printf("Inventory is empty.\n");
        return;
    }
    printf("\n--- SPARE PARTS ---\n");
    for (i = 0; i < pc; i++) {
        printf("ID: %d | %s | Qty: %d | Price: %.2f\n",
               p[i].id, p[i].name, p[i].quantity, p[i].price);
    }
}

int countByStatus(Ticket t[], int tc, int index, int status) {
    if (index >= tc) {
        return 0;
    }
    if (t[index].status == status) {
        return 1 + countByStatus(t, tc, index + 1, status);
    }
    return countByStatus(t, tc, index + 1, status);
}

void showStats(Ticket t[], int tc) {
    int s, i;
    float revenue;

    printf("\n--- STATISTICS ---\n");
    printf("Total tickets: %d\n", tc);
    for (s = 0; s < 4; s++) {
        revenue = 0;
        for (i = 0; i < tc; i++) {
            if (t[i].status == s) {
                revenue += t[i].cost;
            }
        }
        printf("%-10s : %d tickets | Cost total = %.2f\n",
               statusName(s), countByStatus(t, tc, 0, s), revenue);
    }
    revenue = 0;
    for (i = 0; i < tc; i++) {
        if (t[i].status == 3) {
            revenue += t[i].cost;
        }
    }
    printf("Revenue (Collected tickets only) = %.2f\n", revenue);
}

void showGraph(Ticket t[], int tc) {
    char names[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int counts[12] = {0};
    int i, j;

    for (i = 0; i < tc; i++) {
        counts[t[i].month - 1]++;
    }
    printf("\n--- MONTHLY REPAIRS ---\n");
    for (i = 0; i < 12; i++) {
        printf("%s (%2d) | ", names[i], counts[i]);
        for (j = 0; j < counts[i]; j++) {
            printf("*");
        }
        printf("\n");
    }
}

void saveData(Ticket t[], int tc, Part p[], int pc) {
    (void)t; (void)tc; (void)p; (void)pc;
    if (!saveAllChecked()) printf("Error: could not save application data.\n");
}


void loadData(Ticket t[], int *tc, Part p[], int *pc) {
    FILE *f; long bytes; int i;
    *tc=0; *pc=0; ticketsFileOK=1; partsFileOK=1;
    f=fopen("tickets.dat","rb");
    if(f){ if(fseek(f,0,SEEK_END)!=0 || (bytes=ftell(f))<0 || bytes%(long)sizeof(Ticket)!=0 || bytes/(long)sizeof(Ticket)>MAX || fseek(f,0,SEEK_SET)!=0){ticketsFileOK=0;fclose(f);} else { *tc=(int)fread(t,sizeof(Ticket),MAX,f); if(ferror(f))ticketsFileOK=0; fclose(f); for(i=0;i<*tc;i++){if(t[i].status<0||t[i].status>3||t[i].month<1||t[i].month>12||!memchr(t[i].name,0,sizeof(t[i].name))||!memchr(t[i].phone,0,sizeof(t[i].phone))||!memchr(t[i].device,0,sizeof(t[i].device))||!memchr(t[i].issue,0,sizeof(t[i].issue))||t[i].cost<0){ticketsFileOK=0;*tc=0;break;}}} }
    f=fopen("parts.dat","rb");
    if(f){ if(fseek(f,0,SEEK_END)!=0 || (bytes=ftell(f))<0 || bytes%(long)sizeof(Part)!=0 || bytes/(long)sizeof(Part)>MAX || fseek(f,0,SEEK_SET)!=0){partsFileOK=0;fclose(f);} else { *pc=(int)fread(p,sizeof(Part),MAX,f);if(ferror(f))partsFileOK=0;fclose(f);for(i=0;i<*pc;i++){if(p[i].quantity<0||p[i].price<0||!memchr(p[i].name,0,sizeof(p[i].name))){partsFileOK=0;*pc=0;break;}}} }
    /* File errors are reported by /api/health; keep the API console quiet. */
}

