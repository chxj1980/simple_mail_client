# simple_mail_client
简单精巧邮件发送客户端

# feature
- [x] 低内存占用
- [x] 支持发送邮件
- [x] 体积小
- [x] 不到500行代码
- [x] 支持网易163服务器
- [x] 实现smtp

# 编译
- mkdir build
- cmake ..
- make -j10

# sample
[示例代码](./src/sample.c)

# 运行
- 设置环境变量
```
export SMTP_SERVER=smtp服务器地址
export SMTP_PORT=stmp服务器端口
export SMTP_USER=你的用户名
export SMPT_PASSWD=你的密码
export SMTP_FROM=发送者的邮箱地址
export SMTP_TO=接收者的邮箱地址
export SMTP_SUBJET=邮件标题
export SMTP_MESSAGE=邮件正文
```
- 运行
```
./app
```

# author
- treeswayinwind@gmail.com
- 企鹅: 279191230
