#include <stdlib.h>
#include "sendmail.h"

#define GET_ENV(var, env) do { \
    char *e = getenv(#env); \
                            \
    if (!e) { \
        LOGE("get env : %s error", #env); \
        return 0; \
    }  \
    mail_info.var = e;  \
} while(0)

int main()
{
    mail_info_t mail_info;

    GET_ENV(smtp_server, SMTP_SERVER);
    GET_ENV(port, SMTP_PORT);
    GET_ENV(user, SMTP_USER);
    GET_ENV(passwd, SMTP_PASSWD);
    GET_ENV(from, SMTP_FROM);
    GET_ENV(to, SMTP_TO);
    GET_ENV(subject, SMTP_SUBJET);
    GET_ENV(msg, SMTP_MESSAGE);
    if (sendmail(&mail_info) < 0) {
        LOGE("send mail error");
        return 0;
    }

    return 0;
}