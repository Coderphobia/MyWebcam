#ifndef MYWIN_H
#define MYWIN_H

#include <QObject>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTcpSocket>

class MyWin: public QWidget
{
    Q_OBJECT
private:
    /* ui */
    QPushButton *btn_connect;
    QLineEdit *line_ip, *line_port;  
    QLabel *ip, *port, *video;
    QHBoxLayout *hlayout1, *hlayout2;
    QVBoxLayout *vlayout;
    /* jpg */
    unsigned char *data;
    int recv_len;
    /* socket */
    QTcpSocket *tcp_socket;
public:
    explicit MyWin(QWidget *parent = NULL);
    ~MyWin();
public slots:   
    void slot_tcp_connect();
    void slot_tcp_read();
};

#endif /* MYWIN_H */
