/**
* @file src/sendmail.c
* @author rigensen
* @brief 
* @date 日  8/ 2 10:20:48 2020
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "sendmail.h"

#define ARRSZ(arr) (sizeof(arr))/sizeof(arr[0])
#define STATE_QUIT 7

typedef struct {
    int (*on_read)(mail_info_t *mail_info, char *buf, size_t size);
    int (*on_write)(mail_info_t *mail_info, char *buf, size_t *size);
} cmd_t;

const char b64_alphabet[65] = { 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/=" };
    
static char * base64_encode(const char *text) 
{
  char *buffer = NULL;
  char *point = NULL;
  int inlen = 0;
  int outlen = 0;

  if (text == NULL)
    return NULL;
  
  inlen = strlen( text );
  if (inlen == 0) {
      buffer = malloc(sizeof(char));
      buffer[0] = '\0';
      return buffer;
    }
  outlen = (inlen*4)/3;
  if( (inlen % 3) > 0 ) /* got to pad */
    outlen += 4 - (inlen % 3);
  
  buffer = malloc( outlen + 1 ); /* +1 for the \0 */
  memset(buffer, 0, outlen + 1); /* initialize to zero */
  
  for( point=buffer; inlen>=3; inlen-=3, text+=3 ) {
    *(point++) = b64_alphabet[ *text>>2 ]; 
    *(point++) = b64_alphabet[ (*text<<4 & 0x30) | *(text+1)>>4 ]; 
    *(point++) = b64_alphabet[ (*(text+1)<<2 & 0x3c) | *(text+2)>>6 ];
    *(point++) = b64_alphabet[ *(text+2) & 0x3f ];
  }
  
  if( inlen ) {
    *(point++) = b64_alphabet[ *text>>2 ];
    *(point++) = b64_alphabet[ (*text<<4 & 0x30) |
			     (inlen==2?*(text+1)>>4:0) ]; 
    *(point++) = (inlen==1?'=':b64_alphabet[ *(text+1)<<2 & 0x3c ] );
    *(point++) = '=';
  }
  
  *point = '\0';
  return buffer;
}

static int start_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    strncpy(buf, "EHLO smtp\r\n", *size);
    *size = strlen("EHLO smtp\r\n");
    return 0;
}

static int login_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;
    char *b64 = base64_encode(mail_info->user);

    if (!b64) {
        LOGE("base64 encode error;%s", mail_info->user);
        return -1;
    }
    ret = snprintf(buf, *size, "AUTH LOGIN %s\r\n", b64);
    if (ret > *size) {
        LOGE("buf overflow");
        return -1;
    }
    *size = ret;
    return 0;
}

static int login_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "334 UGFzc3dvcmQ6\r\n", size)) {
        LOGE("login fail");
        return -1;
    }
    return 0;
}

static int passwd_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;
    char *b64 = base64_encode(mail_info->passwd);

    if (!b64) {
        LOGE("encode base64 error; %s", mail_info->passwd);
        return -1;
    }

    ret = snprintf(buf, *size, "%s\r\n", b64);
    if (ret > *size) {
        LOGE("buf overfow");
        return -1;
    }
    *size = ret;

    return 0;
}

static int passwd_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "235 Authentication successful\r\n", size)) {
        LOGE("Authentication fail; %s", buf);
        return -1;
    }
    return 0;
}

static int mail_from_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "250 Mail OK\r\n", size)) {
        LOGE("mail from error");
        return -1;
    }
    return 0;
}

static int mail_from_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;

    ret = snprintf(buf, *size, "MAIL FROM:<%s>\r\n", mail_info->from);
    if (ret > *size) {
        LOGE("buf overflow");
        return -1;
    }
    *size = ret;
    return 0;
}

static int rcpt_to_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "250 Mail OK\r\n", size)) {
        LOGE("mail from error");
        return -1;
    }
    return 0;
}

static int rcpt_to_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;

    ret = snprintf(buf, *size, "RCPT TO:<%s>\r\n", mail_info->to);
    if (ret > *size) {
        LOGE("buf overflow");
        return -1;
    }
    *size = ret;
    return 0;
}

static int data_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;

    ret = snprintf(buf, *size, "DATA\r\n");
    if (ret > *size) {
        LOGE("buf overflow");
        return -1;
    }
    *size = ret;
    return 0;
}

static int data_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "354 End data with <CR><LF>.<CR><LF>\r\n", size)) {
        LOGE("DATA error");
        return -1;
    }

    return 0;
}

static void randstr(char* rs, int rs_len)
{
    int i, offset;
    struct timespec time_srand;
    char base[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static int base_len = 26;

    clock_gettime(CLOCK_MONOTONIC, &time_srand);
    srand(time_srand.tv_nsec);

    for (i = 0; i < rs_len; i++) {
            offset = rand() % base_len;
            rs[i] = base[offset];
    }
}

static int msg_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret;
    time_t t = time(NULL);
    char time_buf[256] = {0};
    char boundary[64] = "mime";

    ret = strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S %z", localtime(&t));
    if (!ret) {
        LOGE("strftime error");
        return -1;
    }
    randstr(boundary+strlen(boundary), 10);
    ret = snprintf(buf, *size, "DATE: %s\r\n"
                               "From: <%s>\r\n"
                               "Subject: %s\r\n"
                               "To: <%s>\r\n"
                               "MIME-Version: 1.0\r\n"
                               "Content-Type: multipart/mixed; boundary=%s\r\n\r\n"
                               "Multipart MIME message.\r\n"
                               "--%s\r\n"
                               "Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n"
                               "%s\r\n\r\n"
                               "--%s--\r\n"
                               ".\r\n",
                               time_buf,
                               mail_info->from,
                               mail_info->subject,
                               mail_info->to,
                               boundary,
                               boundary,
                               mail_info->msg,
                               boundary);
    if (ret > *size) {
        LOGE("buffer overflow");
        return -1;
    }
    *size = ret;

    return 0;
}

static int msg_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "250 Mail OK", strlen("250 Mail OK"))) {
        LOGE("msg error");
        return -1;
    }
    return 0;
}

static int quit_on_read(mail_info_t *mail_info, char *buf, size_t size)
{
    if (strncmp(buf, "221 Bye\r\n", size)) {
        LOGE("quit error");
        return -1;
    }
    return 0;
}

static int quit_on_write(mail_info_t *mail_info, char *buf, size_t *size)
{
    int ret ;

    ret = snprintf(buf, *size, "QUIT\r\n");
    if (ret > *size) {
        LOGE("buf overflow");
        return -1;
    }
    return 0;
}

static cmd_t smtp_cmds[] =
{
    {NULL, start_on_write},
    {login_on_read, login_on_write},
    {passwd_on_read, passwd_on_write},
    {mail_from_on_read, mail_from_on_write},
    {rcpt_to_on_read, rcpt_to_on_write},
    {data_on_read, data_on_write},
    {msg_on_read, msg_on_write},
    {quit_on_read, quit_on_write},
};

int sendmail(mail_info_t *mail_info)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *host;
    int state, ret;
    char rbuf[1024] = {0};

    if (!mail_info) {
        LOGE("check param error");
        return -1;
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOGE("create socket error");
        return -1;
    }
    host = gethostbyname(mail_info->smtp_server);
    if (!host) {
        LOGE("get ip of %s error", mail_info->smtp_server);
        return -1;
    }
    LOGI("ip of %s : %s", mail_info->smtp_server, inet_ntoa(*((struct in_addr*)host->h_addr)));
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)host->h_addr, (char *)&serv_addr.sin_addr.s_addr, host->h_length);
    serv_addr.sin_port = htons(atoi(mail_info->port));
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        LOGE("connect to %s error", mail_info->smtp_server);
        return -1;
    }
    // 220 163.com Anti-spam GT for Coremail System (163com[20141201])
    ret = read(sockfd, rbuf, sizeof(rbuf));
    if (ret < 0) {
        LOGE("read error");
        return -1;
    }
    LOGI("S: %s", rbuf);
    while(state <= STATE_QUIT) {
        char wbuf[1024] = {0};
        size_t size = sizeof(wbuf);
        cmd_t *pcmd = &smtp_cmds[state++];

        if (!pcmd->on_write) {
            LOGE("check state: %d on_write error", state-1);
            return -1;
        }
        if (pcmd->on_write(mail_info, wbuf, &size)) {
            LOGE("state: %d on_write error", state-1);
            return -1;
        }
        LOGI("C: %s", wbuf);
        ret = write(sockfd, wbuf, size);
        if (ret < 0) {
            LOGE("write error");
            return -1;
        }
        memset(rbuf, 0, sizeof(rbuf));
        ret = read(sockfd, rbuf, sizeof(rbuf));
        if (ret < 0) {
            LOGE("read error");
            return -1;
        }
        LOGI("S: %s", rbuf);
        if (pcmd->on_read && pcmd->on_read(mail_info, rbuf, ret)) {
            LOGE("state: %d on_read error", state-1);
            return -1;
        }
    }

    LOGI("send mail success");

    return 0;
}

