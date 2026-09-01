#include "spotify_search.h"
#include "spotify_config.h"

#include <psp2/net/net.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int url_encode(const char *in, char *out, size_t n)
{
    static const char h[]="0123456789ABCDEF";
    size_t w=0;
    for (const unsigned char *p=(const unsigned char*)in; *p; ++p) {
        unsigned char c=*p;
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
            c=='-'||c=='_'||c=='.'||c=='~') {
            if (w+1>=n) return -1;
            out[w++]=(char)c;
        } else {
            if (w+3>=n) return -1;
            out[w++]='%'; out[w++]=h[(c>>4)&15]; out[w++]=h[c&15];
        }
    }
    out[w]='\0';
    return 0;
}

static int http_get_proxy(const char *path, char *body, size_t body_cap, int *status)
{
    int s=sceNetSocket("spotify-proxy", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (s<0) return s;

    SceNetSockaddrIn addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=SCE_NET_AF_INET;
    addr.sin_port=sceNetHtons(SPOTIFY_PC_PROXY_PORT);
    if (sceNetInetPton(SCE_NET_AF_INET, SPOTIFY_PC_PROXY_HOST, &addr.sin_addr) <= 0) {
        sceNetSocketClose(s); return -5001;
    }

    int rc=sceNetConnect(s,(SceNetSockaddr*)&addr,sizeof(addr));
    if (rc<0) { sceNetSocketClose(s); return rc; }

    char req[768];
    int rn=snprintf(req,sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
        path, SPOTIFY_PC_PROXY_HOST, SPOTIFY_PC_PROXY_PORT);
    rc=sceNetSend(s,req,rn,0);
    if (rc<0) { sceNetSocketClose(s); return rc; }

    char resp[16384];
    size_t used=0;
    while (used+1<sizeof(resp)) {
        rc=sceNetRecv(s,resp+used,sizeof(resp)-used-1,0);
        if (rc<=0) break;
        used += (size_t)rc;
    }
    sceNetSocketClose(s);
    resp[used]='\0';

    int st=0;
    sscanf(resp,"HTTP/%*s %d",&st);
    if (status) *status=st;

    char *p=strstr(resp,"\r\n\r\n");
    if (!p) return -5002;
    p += 4;
    snprintf(body,body_cap,"%s",p);
    return 0;
}

/* Tiny JSON extractor for companion response:
   {"tracks":[{"name":"...","artist":"...","uri":"..."}, ...]} */
static int copy_json_string(const char *obj, const char *key, char *out, size_t cap)
{
    char pat[64];
    snprintf(pat,sizeof(pat),"\"%s\":\"",key);
    const char *p=strstr(obj,pat);
    if (!p) return -1;
    p += strlen(pat);
    size_t w=0;
    while (*p && *p!='"' && w+1<cap) {
        if (*p=='\\' && p[1]) {
            ++p;
            if (*p=='n') out[w++]=' ';
            else out[w++]=*p;
            ++p;
        } else {
            out[w++]=*p++;
        }
    }
    out[w]='\0';
    return 0;
}

int spotify_search_tracks(
    SpotifyAuthPkce *auth,
    const char *query,
    SpotifyTrack *results,
    int max_results,
    int *out_count,
    int *out_http_status
)
{
    (void)auth;
    if (!query||!results||!out_count||max_results<=0) return -1;
    *out_count=0;

    char q[384], path[512], body[12000];
    if (url_encode(query,q,sizeof(q))<0) return -2;
    snprintf(path,sizeof(path),"/search?q=%s",q);

    int status=0;
    int rc=http_get_proxy(path,body,sizeof(body),&status);
    if (out_http_status) *out_http_status=status;
    if (rc<0) return rc;
    if (status!=200) return -status;

    const char *p=body;
    while (*out_count<max_results && (p=strstr(p,"{\"name\":\""))!=NULL) {
        SpotifyTrack *t=&results[*out_count];
        memset(t,0,sizeof(*t));
        copy_json_string(p,"name",t->title,sizeof(t->title));
        copy_json_string(p,"artist",t->artist,sizeof(t->artist));
        copy_json_string(p,"uri",t->uri,sizeof(t->uri));
        t->valid = t->uri[0] ? 1 : 0;
        ++(*out_count);
        ++p;
    }
    return 0;
}

/* Shared helper for playback through PC companion. */
int spotify_proxy_simple_get(const char *path, int *out_http_status)
{
    char body[1024];
    int status=0;
    int rc=http_get_proxy(path,body,sizeof(body),&status);
    if (out_http_status) *out_http_status=status;
    if (rc<0) return rc;
    return status==200 ? 0 : -status;
}
