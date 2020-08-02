/**
* @file sendmail.h
* @author rigensen
* @brief 
* @date 日  8/ 2 10:21:36 2020
*/

#ifndef _SENDMAIL_H

#include <stdio.h>

typedef struct {
    char *smtp_server;
    char *port;
    char *user;
    char *passwd;
    char *from;
    char *to;
    char *subject;
    char *msg;
} mail_info_t;

extern int sendmail(mail_info_t *mail_info);

#ifndef __FILENAME__
    #define __FILENAME__ __FILE__
#endif

#define LOGI(fmt, args...) printf("%s:%d(%s) $ "fmt"\n", __FILENAME__, __LINE__, __FUNCTION__, ##args)
#define LOGE(fmt, args...) printf("\e[0;31m%s:%d(%s)$ "fmt"\n\e[0m", __FILENAME__, __LINE__, __FUNCTION__, ##args)


#define _SENDMAIL_H
#endif
