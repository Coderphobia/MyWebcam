
#include "mywin.h"
#include <QString>
#include <QMessageBox>
#include <QPixmap>
#include <QDebug>

#define WIDTH  640
#define HEIGHT 480

#define P_LEN  1300
#define LEN    (WIDTH*HEIGHT*2)

#define PORT   "5566"
#define IP     "192.168.250.234"

MyWin::MyWin(QWidget *parent) : QWidget(parent)
{
    /* set ui */
    resize(WIDTH, HEIGHT+10);
    vlayout = new QVBoxLayout(this);
    hlayout1 = new QHBoxLayout();
    hlayout2 = new QHBoxLayout();


    ip = new QLabel(QString("IP:"));
    line_ip = new QLineEdit(IP);
    line_ip->setClearButtonEnabled(true);
    hlayout1->addWidget(ip, 1);
    hlayout1->addWidget(line_ip, 3);

    port = new QLabel(QString("PORT:"));
    line_port = new QLineEdit(PORT);
    line_port->setClearButtonEnabled(true);
    hlayout1->addWidget(port, 1);
    hlayout1->addWidget(line_port, 3);

    btn_connect = new QPushButton(QString("connect"));
    hlayout1->addWidget(btn_connect, 2);

    video = new QLabel();
    video->setGeometry(0, 0, WIDTH, HEIGHT);
    hlayout2->addWidget(video);

    vlayout->addLayout(hlayout2, 8);
    vlayout->addLayout(hlayout1, 2);

    /* allocate data memory */
    data = new unsigned char[LEN];

    /* create socket */
    tcp_socket = new QTcpSocket(this);

    /* signal-slot connections */
    connect(btn_connect, SIGNAL(clicked()), this, SLOT(slot_tcp_connect()));
    connect(tcp_socket, SIGNAL(readyRead()), this, SLOT(slot_tcp_read()));
}

MyWin::~MyWin()
{
    delete ip;
    delete port;
    delete btn_connect;
    delete line_ip;
    delete line_port;
    delete hlayout1;
    delete hlayout2;
    delete vlayout;
    delete tcp_socket;
    delete video;
    delete [] data;
}


void MyWin::slot_tcp_read()
{
#if 1
    int ret;

    ret = tcp_socket->read((char*)(data+recv_len), P_LEN);
    if (ret <= 0)
        return;

    recv_len += ret;
    if (ret != P_LEN) /* last part of the jpg is recieved */
    {
        QPixmap map;
        map.loadFromData(data, recv_len);
        video->setPixmap(map); /* show it */
        recv_len = 0;
        tcp_socket->write("REQUEST"); /* request new jpg */
        return;
    }
    tcp_socket->write("ACK"); /* confirm work for each recieved P_LEN of the jpg */
#endif
}


void MyWin::slot_tcp_connect()
{
    /* check current connection state */
    if (!QString::compare(btn_connect->text(), QString("connected")))
    {
        /* disconnect ? */
        if (QMessageBox::question(this, "confirm", "disconnect?") \
            == QMessageBox::StandardButton::Yes)
        {
            tcp_socket->disconnectFromHost(); /* yes! */
            //tcp_socket->close();
            btn_connect->setText("connect");
        }
        return;
    }

    /* request to connect */
    QString ip = line_ip->text();
    quint16 port = line_port->text().toUShort();
    tcp_socket->connectToHost(ip, port);
    if (!tcp_socket->waitForConnected(1000))
    {
        QMessageBox::critical(this, "error", "connection failed");
    }
    else
    {
        btn_connect->setText(QString("connected"));
        tcp_socket->write("REQUEST"); /* request first jpg */
    }
}
